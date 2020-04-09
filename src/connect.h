#pragma once

#include "esp_err.h"

esp_err_t wifi_connect();

esp_err_t wifi_disconnect();

esp_err_t wifi_credentials(const char *ssid, const char *pass);
