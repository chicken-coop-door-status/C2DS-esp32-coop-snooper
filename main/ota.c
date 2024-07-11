#include "ota.h"
#include "sdkconfig.h"
#include <inttypes.h>
#include "esp_timer.h"

extern const uint8_t AmazonRootCA1_pem[];

static const char *TAG = "OTA";

#define MAX_RETRIES 5
#define LOG_PROGRESS_INTERVAL 100

void ota_task(void *pvParameter)
{
    esp_err_t ota_finish_err = ESP_OK;
    esp_http_client_config_t config = {
        .url = CONFIG_OTA_URL,
        .cert_pem = (char *)AmazonRootCA1_pem,
        .timeout_ms = 30000, // Increased timeout
    };

    esp_https_ota_config_t ota_config = {
        .http_config = &config,
    };

    esp_https_ota_handle_t ota_handle = NULL;
    esp_err_t err = esp_https_ota_begin(&ota_config, &ota_handle);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "Failed to start OTA: %s", esp_err_to_name(err));
        vTaskDelete(NULL);
        return;
    }

    int64_t start_time = esp_timer_get_time();
    int retries = 0;
    int loop_count = 0;
    while (1) {
        err = esp_https_ota_perform(ota_handle);
        if (err == ESP_ERR_HTTPS_OTA_IN_PROGRESS) {
            if (loop_count % LOG_PROGRESS_INTERVAL == 0) {
                ESP_LOGI(TAG, "OTA in progress... Free heap size: %" PRIu32, esp_get_free_heap_size());
            }
            loop_count++;
        } else if (err != ESP_OK) {
            ESP_LOGE(TAG, "OTA perform error: %s", esp_err_to_name(err));
            if (++retries > MAX_RETRIES) {
                ESP_LOGE(TAG, "Max retries reached, aborting OTA");
                break;
            }
            ESP_LOGI(TAG, "Retrying OTA...");
        } else {
            break;
        }

        vTaskDelay(1000 / portTICK_PERIOD_MS); // Small delay before retrying
    }

    if (esp_https_ota_is_complete_data_received(ota_handle)) {
        ota_finish_err = esp_https_ota_finish(ota_handle);
        if (ota_finish_err == ESP_OK) {
            int64_t end_time = esp_timer_get_time();
            int64_t duration_us = end_time - start_time;

            int duration_s = duration_us / 1000000;
            int hours = duration_s / 3600;
            int minutes = (duration_s % 3600) / 60;
            int seconds = duration_s % 60;

            ESP_LOGI(TAG, "OTA update successful, duration: %02d:%02d:%02d, restarting...", hours, minutes, seconds);
            esp_restart();
        } else {
            ESP_LOGE(TAG, "OTA update failed: %s", esp_err_to_name(ota_finish_err));
        }
    } else {
        ESP_LOGE(TAG, "Complete data was not received.");
    }

    vTaskDelete(NULL);
}