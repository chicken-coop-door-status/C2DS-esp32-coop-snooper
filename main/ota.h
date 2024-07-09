#ifndef OTA_UPDATE_H
#define OTA_UPDATE_H

#include <stdio.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_system.h"
#include "esp_log.h"
#include "nvs_flash.h"
#include "esp_ota_ops.h"
#include "esp_https_ota.h"
#include "esp_event.h"
#include "esp_netif.h"
#include "esp_wifi.h"
#include "esp_http_client.h"
#include "mqtt_client.h"

void ota_task(void *pvParameter);
#endif // OTA_UPDATE_H
