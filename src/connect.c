/* Common functions for protocol examples, to establish Wi-Fi or Ethernet
   connection.

   This example code is in the Public Domain (or CC0 licensed, at your option.)

   Unless required by applicable law or agreed to in writing, this
   software is distributed on an "AS IS" BASIS, WITHOUT WARRANTIES OR
   CONDITIONS OF ANY KIND, either express or implied.
 */

#include "connect.h"
#include "driver/gpio.h"
#include "esp_event.h"
#include "esp_log.h"
#include "esp_wifi.h"
#include "freertos/FreeRTOS.h"
#include "freertos/event_groups.h"
#include "freertos/task.h"
#include "lwip/err.h"
#include "lwip/sys.h"
#include "tcpip_adapter.h"
#include <string.h>

static uint8_t wifi_ssid[32] = {0};
static uint8_t wifi_password[64] = {0};

#define GOT_IPV4_BIT BIT(0)
#define GOT_IPV6_BIT BIT(1)

#define CONNECTED_BITS (GOT_IPV4_BIT | GOT_IPV6_BIT)

static EventGroupHandle_t s_connect_event_group;
static ip4_addr_t s_ip_addr;
static ip6_addr_t s_ipv6_addr;
static const char *s_connection_name;

static const char *TAG = "connect";

static void on_wifi_disconnect(void *arg, esp_event_base_t event_base,
                               int32_t event_id, void *event_data) {
  ESP_LOGI(TAG, "Wi-Fi %s:%s", wifi_ssid, wifi_password);
  ESP_LOGI(TAG, "Wi-Fi disconnected, trying to reconnect...");
  ESP_ERROR_CHECK(esp_wifi_connect());
}

static void on_wifi_connect(void *arg, esp_event_base_t event_base,
                            int32_t event_id, void *event_data) {
  tcpip_adapter_create_ip6_linklocal(TCPIP_ADAPTER_IF_STA);
}

static void on_got_ipv6(void *arg, esp_event_base_t event_base,
                        int32_t event_id, void *event_data) {
  ip_event_got_ip6_t *event = (ip_event_got_ip6_t *)event_data;
  memcpy(&s_ipv6_addr, &event->ip6_info.ip, sizeof(s_ipv6_addr));
  xEventGroupSetBits(s_connect_event_group, GOT_IPV6_BIT);
}

static void on_got_ip(void *arg, esp_event_base_t event_base, int32_t event_id,
                      void *event_data) {
  ip_event_got_ip_t *event = (ip_event_got_ip_t *)event_data;
  memcpy(&s_ip_addr, &event->ip_info.ip, sizeof(s_ip_addr));
  xEventGroupSetBits(s_connect_event_group, GOT_IPV4_BIT);
}

/* set up connection, Wi-Fi or Ethernet */
static void start() {
  wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
  ESP_ERROR_CHECK(esp_wifi_init(&cfg));

  ESP_ERROR_CHECK(esp_event_handler_register(
      WIFI_EVENT, WIFI_EVENT_STA_DISCONNECTED, &on_wifi_disconnect, NULL));
  ESP_ERROR_CHECK(esp_event_handler_register(IP_EVENT, IP_EVENT_STA_GOT_IP,
                                             &on_got_ip, NULL));

  ESP_ERROR_CHECK(esp_event_handler_register(
      WIFI_EVENT, WIFI_EVENT_STA_CONNECTED, &on_wifi_connect, NULL));
  ESP_ERROR_CHECK(esp_event_handler_register(IP_EVENT, IP_EVENT_GOT_IP6,
                                             &on_got_ipv6, NULL));

  ESP_ERROR_CHECK(esp_wifi_set_storage(WIFI_STORAGE_RAM));
  wifi_config_t wifi_config = {};

  memcpy(wifi_config.sta.ssid, wifi_ssid, sizeof(wifi_config.sta.ssid));
  memcpy(wifi_config.sta.password, wifi_password,
         sizeof(wifi_config.sta.password));

  ESP_LOGI(TAG, "Connecting to %s:%s...", wifi_config.sta.ssid,
           wifi_config.sta.password);

  ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_STA));
  ESP_ERROR_CHECK(esp_wifi_set_config(ESP_IF_WIFI_STA, &wifi_config));
  ESP_ERROR_CHECK(esp_wifi_start());
  ESP_ERROR_CHECK(esp_wifi_connect());
  s_connection_name = (const char *)wifi_ssid;
}

/* tear down connection, release resources */
static void stop() {
  ESP_ERROR_CHECK(esp_event_handler_unregister(
      WIFI_EVENT, WIFI_EVENT_STA_DISCONNECTED, &on_wifi_disconnect));
  ESP_ERROR_CHECK(
      esp_event_handler_unregister(IP_EVENT, IP_EVENT_STA_GOT_IP, &on_got_ip));
  ESP_ERROR_CHECK(
      esp_event_handler_unregister(IP_EVENT, IP_EVENT_GOT_IP6, &on_got_ipv6));
  ESP_ERROR_CHECK(esp_event_handler_unregister(
      WIFI_EVENT, WIFI_EVENT_STA_CONNECTED, &on_wifi_connect));
  ESP_ERROR_CHECK(esp_wifi_stop());
  ESP_ERROR_CHECK(esp_wifi_deinit());
}

esp_err_t wifi_credentials(const char *ssid, const char *pass) {
  int len;

  len = snprintf((char *)wifi_ssid, sizeof(wifi_ssid), ssid);
  if (len != strlen(ssid)) {
    return ESP_ERR_INVALID_SIZE;
  }

  len = snprintf((char *)wifi_password, sizeof(wifi_password), pass);
  if (len != strlen(pass)) {
    return ESP_ERR_INVALID_SIZE;
  }

  return ESP_OK;
}

esp_err_t wifi_connect() {
  if (s_connect_event_group != NULL) {
    return ESP_ERR_INVALID_STATE;
  }

  s_connect_event_group = xEventGroupCreate();

  start();

  xEventGroupWaitBits(s_connect_event_group, CONNECTED_BITS, true, true,
                      portMAX_DELAY);
  ESP_LOGI(TAG, "Connected to %s", s_connection_name);
  ESP_LOGI(TAG, "IPv4 address: " IPSTR, IP2STR(&s_ip_addr));
  ESP_LOGI(TAG, "IPv6 address: " IPV6STR, IPV62STR(s_ipv6_addr));
  return ESP_OK;
}

esp_err_t wifi_disconnect() {
  if (s_connect_event_group == NULL) {
    return ESP_ERR_INVALID_STATE;
  }

  vEventGroupDelete(s_connect_event_group);
  s_connect_event_group = NULL;

  stop();

  ESP_LOGI(TAG, "Disconnected from %s", s_connection_name);
  s_connection_name = NULL;
  return ESP_OK;
}
