/*
 * In The Name of God
 * =======================================
 * [] File Name : main.c
 *
 * [] Creation Date : 08-04-2020
 *
 * [] Created By : Parham Alvani <parham.alvani@gmail.com>
 * =======================================
 */
/*
 * Copyright (c)  2020 Parham Alvani.
 */

#include <string.h>
#include <sys/socket.h>

#include "freertos/FreeRTOS.h"
#include "freertos/event_groups.h"
#include "freertos/task.h"

#include "esp_event.h"
#include "esp_log.h"
#include "esp_wifi.h"

#include "nvs_flash.h"

#include "connect.h"

#include "coap.h"
#include "cJSON.h"

/* Set this to 9 to get verbose logging from within libcoap */
#define COAP_LOGGING_LEVEL 0

const static char *TAG = "esp32_demo";
const static int PORT = 1378;

/* Wifi Credentials */
#define DEMO_WIFI_SSID "ssid"
#define DEMO_WIFI_PASSWORD "secret"

/*
 * The resource handler
 */
static void hnd_elahe_get(coap_context_t *ctx, coap_resource_t *resource,
                          coap_session_t *session, coap_pdu_t *request,
                          coap_binary_t *token, coap_string_t *query,
                          coap_pdu_t *response) {
  char *response_data;
  cJSON *response_json = cJSON_CreateObject();

  if (cJSON_AddStringToObject(response_json, "name", "elahe") == NULL) {
    return;
  }

  if (cJSON_AddNumberToObject(response_json, "time", time(NULL)) == NULL) {
    return;
  }

  response_data = cJSON_Print(response_json);

  coap_add_data_blocked_response(
      resource, session, request, response, token, COAP_MEDIATYPE_TEXT_PLAIN, 0,
      strlen(response_data), (const u_char *)response_data);
}

static void coap_goldoon_server(void *p) {
  coap_context_t *ctx = NULL;
  coap_address_t serv_addr;
  coap_resource_t *resource = NULL;

  coap_set_log_level(COAP_LOGGING_LEVEL);

  while (1) {
    coap_endpoint_t *ep = NULL;
    unsigned wait_ms;

    /* Prepare the CoAP server socket */
    coap_address_init(&serv_addr);
    serv_addr.addr.sin.sin_family = AF_INET;
    serv_addr.addr.sin.sin_addr.s_addr = INADDR_ANY;
    serv_addr.addr.sin.sin_port = htons(PORT);

    ctx = coap_new_context(NULL);
    if (!ctx) {
      ESP_LOGE(TAG, "coap_new_context() failed");
      continue;
    }

    ep = coap_new_endpoint(ctx, &serv_addr, COAP_PROTO_UDP);
    if (!ep) {
      ESP_LOGE(TAG, "udp: coap_new_endpoint() failed");
      goto clean_up;
    }

    resource = coap_resource_init(coap_make_str_const("elahe"), 0);
    if (!resource) {
      ESP_LOGE(TAG, "coap_resource_init() failed");
      goto clean_up;
    }

    coap_register_handler(resource, COAP_REQUEST_GET, hnd_elahe_get);
    /* We possibly want to Observe the GETs */
    coap_resource_set_get_observable(resource, 1);
    coap_add_resource(ctx, resource);

    wait_ms = COAP_RESOURCE_CHECK_TIME * 1000;

    while (1) {
      int result = coap_run_once(ctx, wait_ms);
      if (result < 0) {
        break;
      } else if (result && (unsigned)result < wait_ms) {
        /* decrement if there is a result wait time returned */
        wait_ms -= result;
      }
      if (result) {
        /* result must have been >= wait_ms, so reset wait_ms */
        wait_ms = COAP_RESOURCE_CHECK_TIME * 1000;
      }
    }
  }
clean_up:
  coap_free_context(ctx);
  coap_cleanup();

  vTaskDelete(NULL);
}

void app_main(void) {
  ESP_ERROR_CHECK(nvs_flash_init());
  tcpip_adapter_init();
  ESP_ERROR_CHECK(esp_event_loop_create_default());

  ESP_ERROR_CHECK(wifi_credentials(DEMO_WIFI_SSID, DEMO_WIFI_PASSWORD));
  ESP_ERROR_CHECK(wifi_connect());

  // uses mac address as an unique identifier
  uint8_t mac[6];
  ESP_ERROR_CHECK(esp_wifi_get_mac(WIFI_IF_STA, mac));
  ESP_LOGI(TAG, "%02x:%02x:%02x:%02x:%02x:%02x\n", mac[0], mac[1], mac[2],
           mac[3], mac[4], mac[5]);

  /*
   * Internally, within the FreeRTOS implementation, tasks use two blocks of
   * memory.  The first block is used to hold the task's data structures.  The
   * second block is used by the task as its stack.  If a task is created using
   * xTaskCreate() then both blocks of memory are automatically dynamically
   * allocated inside the xTaskCreate() function.
   */
  xTaskCreate(coap_goldoon_server, "coap", 1024 * 5, NULL, 5, NULL);
}
