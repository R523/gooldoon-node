/*
 * In The Name Of God
 * ========================================
 * [] File Name : main.c
 *
 * [] Creation Date : 10-05-2018
 *
 * [] Created By : Parham Alvani <parham.alvani@gmail.com>
 * =======================================
 */
/*
 * Copyright (c)  2018 Parham Alvani.
 */
#include <string.h>
#include <sys/socket.h>

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/event_groups.h"

#include "esp_log.h"
#include "esp_wifi.h"
#include "esp_event_loop.h"

#include "nvs_flash.h"

#include "coap.h"

#define GOLDOON_WIFI_SSID "Parham-Main"
#define GOLDOON_WIFI_PASS "Parham13730321"

#define COAP_DEFAULT_TIME_SEC 5
#define COAP_DEFAULT_TIME_USEC 0

static EventGroupHandle_t wifi_event_group;

/*
 * The event group allows multiple bits for each event,
 * but we only care about one event - are we connected
 * to the AP with an IP?
 */
static const int CONNECTED_BIT = BIT0;

static const char *TAG = "CoAP_server";

static coap_async_state_t *async;

static void send_async_response(coap_context_t *ctx,
		const coap_endpoint_t *local_if)
{
	coap_pdu_t *response;
	unsigned char buf[3];
	const char *response_data = "18.20 is leaving us";

	size_t size = sizeof(coap_hdr_t) + 30;
	response = coap_pdu_init(async->flags & COAP_MESSAGE_CON,
			COAP_RESPONSE_CODE(205), 0, size);
	response->hdr->id = coap_new_message_id(ctx);
	if (async->tokenlen)
		coap_add_token(response, async->tokenlen, async->token);
	coap_add_option(response, COAP_OPTION_CONTENT_TYPE, coap_encode_var_bytes(buf, COAP_MEDIATYPE_TEXT_PLAIN), buf);
	coap_add_data  (response, strlen(response_data), (unsigned char *)response_data);

	if (coap_send(ctx, local_if, &async->peer, response) == COAP_INVALID_TID) {
	}

	coap_delete_pdu(response);
	coap_async_state_t *tmp;
	coap_remove_async(ctx, async->id, &tmp);
	coap_free_async(async);
	async = NULL;
}

/*
 * The resource handler
 */
static void async_handler(coap_context_t *ctx, struct coap_resource_t *resource,
		const coap_endpoint_t *local_interface, coap_address_t *peer,
		coap_pdu_t *request, str *token, coap_pdu_t *response)
{
	async = coap_register_async(ctx, peer, request, COAP_ASYNC_SEPARATE | COAP_ASYNC_CONFIRM, (void*)"no data");
}

static void coap_goldoon_thread(void *p)
{
	coap_context_t*  ctx = NULL;
	coap_address_t   serv_addr;
	coap_resource_t* resource = NULL;
	fd_set           readfds;
	struct timeval tv;
	int flags = 0;

	while (1) {
		/*
		 * Wait for the callback to set the CONNECTED_BIT in the
		 * event group.
		 */
		xEventGroupWaitBits(wifi_event_group, CONNECTED_BIT,
				false, true, portMAX_DELAY);
		ESP_LOGI(TAG, "Connected to AP");

		/* Prepare the CoAP server socket */
		coap_address_init(&serv_addr);
		serv_addr.addr.sin.sin_family      = AF_INET;
		serv_addr.addr.sin.sin_addr.s_addr = INADDR_ANY;
		serv_addr.addr.sin.sin_port        = htons(COAP_DEFAULT_PORT);
		ctx                                = coap_new_context(&serv_addr);
		if (ctx) {
			flags = fcntl(ctx->sockfd, F_GETFL, 0);
			fcntl(ctx->sockfd, F_SETFL, flags|O_NONBLOCK);

			tv.tv_usec = COAP_DEFAULT_TIME_USEC;
			tv.tv_sec = COAP_DEFAULT_TIME_SEC;
			/* Initialize the resource */
			resource = coap_resource_init((unsigned char *)"About", 9, 0);
			if (resource){
				coap_register_handler(resource, COAP_REQUEST_GET, async_handler);
				coap_add_resource(ctx, resource);
				/*For incoming connections*/
				for (;;) {
					FD_ZERO(&readfds);
					FD_CLR( ctx->sockfd, &readfds);
					FD_SET( ctx->sockfd, &readfds);

					int result = select( ctx->sockfd+1, &readfds, 0, 0, &tv );
					if (result > 0){
						if (FD_ISSET( ctx->sockfd, &readfds ))
							coap_read(ctx);
					} else if (result < 0){
						break;
					} else {
						ESP_LOGE(TAG, "select timeout");
					}

					if (async) {
						send_async_response(ctx, ctx->endpoint);
					}
				}
			}

			coap_free_context(ctx);
		}
	}

	vTaskDelete(NULL);
}

static esp_err_t wifi_event_handler(void *ctx, system_event_t *event)
{
	switch(event->event_id) {
		case SYSTEM_EVENT_STA_START:
			esp_wifi_connect();
			break;
		case SYSTEM_EVENT_STA_GOT_IP:
			xEventGroupSetBits(wifi_event_group, CONNECTED_BIT);
			break;
		case SYSTEM_EVENT_STA_DISCONNECTED:
			/*
			 * This is a workaround as ESP32 WiFi libs don't currently
			 * auto-reassociate.
			 */
			esp_wifi_connect();
			xEventGroupClearBits(wifi_event_group, CONNECTED_BIT);
			break;
		default:
			break;
	}
	return ESP_OK;
}

static void wifi_conn_init(void)
{
	tcpip_adapter_init();
	wifi_event_group = xEventGroupCreate();
	ESP_ERROR_CHECK(esp_event_loop_init(wifi_event_handler, NULL));
	wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
	ESP_ERROR_CHECK(esp_wifi_init(&cfg));
	ESP_ERROR_CHECK(esp_wifi_set_storage(WIFI_STORAGE_RAM));
	printf("%s - %s\n", GOLDOON_WIFI_SSID, GOLDOON_WIFI_PASS);
	wifi_config_t wifi_config = {
		.sta = {
			.ssid = GOLDOON_WIFI_SSID,
			.password = GOLDOON_WIFI_PASS,
		},
	};
	// WIFI_MODE_STA -> WiFi station mode
	ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_STA));
	ESP_ERROR_CHECK(esp_wifi_set_config(WIFI_IF_STA, &wifi_config));
	ESP_ERROR_CHECK(esp_wifi_start());
}

void app_main(void)
{
	ESP_ERROR_CHECK(nvs_flash_init());
	wifi_conn_init();
	// uses mac address as an unique identifier
	uint8_t mac[6];
	ESP_ERROR_CHECK(esp_wifi_get_mac(WIFI_IF_STA, mac));
	printf("%02x:%02x:%02x:%02x:%02x:%02x\n", mac[0], mac[1], mac[2], mac[3], mac[4], mac[5]);

	/*
	 * Internally, within the FreeRTOS implementation, tasks use two blocks of
 	 * memory.  The first block is used to hold the task's data structures.  The
 	 * second block is used by the task as its stack.  If a task is created using
 	 * xTaskCreate() then both blocks of memory are automatically dynamically
 	 * allocated inside the xTaskCreate() function.
 	 */
	xTaskCreate(coap_goldoon_thread, "coap", 2048, NULL, 5, NULL);
}
