#include "ota.h"

#include <inttypes.h>

#include "cJSON.h"
#include "esp_sleep.h"
#include "esp_timer.h"
#include "led.h"
#include "mqtt.h"
#include "sdkconfig.h"

extern const uint8_t AmazonRootCA1_pem[];

static const char *TAG = "OTA";
#ifdef TENNIS_HOUSE
static const char *HOST_KEY = "tennis-house";
#elif defined(FARM_HOUSE)
static const char *HOST_KEY = "farm-house";
#endif

#define MAX_RETRIES 5
#define LOG_PROGRESS_INTERVAL 100

bool was_booted_after_ota_update(void) {
    esp_reset_reason_t reset_reason = esp_reset_reason();

    if (reset_reason != ESP_RST_SW) {
        return false;
    }

    nvs_handle_t nvs_handle;
    esp_err_t err = nvs_open("storage", NVS_READWRITE, &nvs_handle);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "Failed to open NVS handle: %s", esp_err_to_name(err));
        return false;
    }

    const esp_partition_t *running_partition = esp_ota_get_running_partition();
    const esp_partition_t *boot_partition = esp_ota_get_boot_partition();

    if (running_partition == NULL || boot_partition == NULL) {
        ESP_LOGE(TAG, "Failed to get partition information.");
        nvs_close(nvs_handle);
        return false;
    }

    uint32_t saved_boot_part_addr = 0;
    size_t len = sizeof(saved_boot_part_addr);
    err = nvs_get_u32(nvs_handle, "boot_part", &saved_boot_part_addr);

    if (err == ESP_ERR_NVS_NOT_FOUND) {
        // First boot after update
        ESP_LOGI(TAG, "No saved boot partition address found. Saving current boot partition.");
        err = nvs_set_u32(nvs_handle, "boot_part", boot_partition->address);
        if (err != ESP_OK) {
            ESP_LOGE(TAG, "Failed to save boot partition address: %s", esp_err_to_name(err));
        }
        nvs_commit(nvs_handle);
        nvs_close(nvs_handle);
        return true;
    } else if (err != ESP_OK) {
        ESP_LOGE(TAG, "Failed to get boot partition address: %s", esp_err_to_name(err));
        nvs_close(nvs_handle);
        return false;
    }

    bool is_ota_update = (boot_partition->address != saved_boot_part_addr);
    if (is_ota_update) {
        ESP_LOGI(TAG, "OTA update detected.");
        err = nvs_set_u32(nvs_handle, "boot_part", boot_partition->address);
        if (err != ESP_OK) {
            ESP_LOGE(TAG, "Failed to save boot partition address: %s", esp_err_to_name(err));
        }
        nvs_commit(nvs_handle);
    } else {
        ESP_LOGI(TAG, "No OTA update detected.");
    }

    nvs_close(nvs_handle);
    return is_ota_update;
}

void convert_seconds(int totalSeconds, int *minutes, int *seconds) {
    *minutes = totalSeconds / 60;
    *seconds = totalSeconds % 60;
}

void ota_task(void *pvParameter) {
    ESP_LOGI(TAG, "Starting OTA task");

    char buffer[128];
    esp_err_t ota_finish_err = ESP_OK;

    // Parse the JSON string passed as a parameter
    const char *json_string = (const char *)pvParameter;
    cJSON *json = cJSON_Parse(json_string);
    if (json == NULL) {
        ESP_LOGE(TAG, "Failed to parse JSON string");
        vTaskDelete(NULL);
        return;
    }

    // Get the value for the HOST_KEY key
    cJSON *host_key = cJSON_GetObjectItem(json, HOST_KEY);
    if (host_key == NULL) {
        ESP_LOGE(TAG, "Key '%s' not found in JSON string", HOST_KEY);
        cJSON_Delete(json);
        vTaskDelete(NULL);
        return;
    }

    const char *host_key_value = cJSON_GetStringValue(host_key);
    if (host_key_value == NULL) {
        ESP_LOGE(TAG, "Failed to get value for '%s'", HOST_KEY);
        cJSON_Delete(json);
        vTaskDelete(NULL);
        return;
    }

    ESP_LOGI(TAG, "Host key value: %s", host_key_value);

    esp_http_client_config_t config = {
        .url = host_key_value,  // Use the retrieved value as the OTA URL
        .cert_pem = (char *)AmazonRootCA1_pem,
        .timeout_ms = 30000,  // Increased timeout
    };

    cJSON_Delete(json);

    ESP_LOGI(TAG, "Starting OTA with URL: %s", config.url);

    esp_https_ota_config_t ota_config = {
        .http_config = &config,
    };

    ESP_LOGI(TAG, "Getting MQTT client handle");
    esp_mqtt_client_handle_t my_mqtt_client = (esp_mqtt_client_handle_t)pvParameter;

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
    int loop_minutes = 0;
    int loop_seconds = 0;

    // Get the next update partition
    const esp_partition_t *update_partition = esp_ota_get_next_update_partition(NULL);
    if (update_partition == NULL) {
        ESP_LOGE(TAG, "Failed to find update partition");
        vTaskDelete(NULL);
        return;
    }

    ESP_LOGI(TAG, "OTA update partition: %s", update_partition->label);

    while (1) {
        err = esp_https_ota_perform(ota_handle);
        if (err == ESP_ERR_HTTPS_OTA_IN_PROGRESS) {
            if (loop_count % LOG_PROGRESS_INTERVAL == 0) {
                current_led_state = LED_FLASHING_GREEN;
                convert_seconds(loop_count, &loop_minutes, &loop_seconds);
                cJSON *root = cJSON_CreateObject();
                sprintf(buffer, "%02d:%02d elapsed...", loop_minutes, loop_seconds);
                cJSON_AddStringToObject(root, WIFI_HOSTNAME, buffer);
                const char *json_string = cJSON_Print(root);
                ESP_LOGW(TAG, "Copying image to %s. %s", update_partition->label, buffer);
                esp_mqtt_client_publish(my_mqtt_client, CONFIG_MQTT_PUBLISH_OTA_PROGRESS_TOPIC, json_string, 0, 1, 0);
                cJSON_Delete(root);
                free((void *)json_string);
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

        vTaskDelay(1000 / portTICK_PERIOD_MS);  // Small delay before retrying
    }

    if (esp_https_ota_is_complete_data_received(ota_handle)) {
        ota_finish_err = esp_https_ota_finish(ota_handle);
        if (ota_finish_err == ESP_OK) {
            // Set the new partition as the boot partition
            err = esp_ota_set_boot_partition(update_partition);
            if (err != ESP_OK) {
                ESP_LOGE(TAG, "Failed to set boot partition: %s", esp_err_to_name(err));
                vTaskDelete(NULL);
                return;
            }

            int64_t end_time = esp_timer_get_time();
            int64_t duration_us = end_time - start_time;

            int duration_s = duration_us / 1000000;
            int hours = duration_s / 3600;
            int minutes = (duration_s % 3600) / 60;
            int seconds = duration_s % 60;

            cJSON *root = cJSON_CreateObject();
            sprintf(buffer, "OTA COMPLETED. Duration: %02d:%02d:%02d", hours, minutes, seconds);
            cJSON_AddStringToObject(root, WIFI_HOSTNAME, buffer);
            const char *json_string = cJSON_Print(root);
            ESP_LOGI(TAG, "Image copy successful. Duration: %02d:%02d:%02d. Will reboot from partition %s", hours,
                     minutes, seconds, update_partition->label);
            esp_mqtt_client_publish(my_mqtt_client, CONFIG_MQTT_PUBLISH_OTA_PROGRESS_TOPIC, json_string, 0, 1, 0);
            cJSON_Delete(root);
            free((void *)json_string);
            if (my_mqtt_client != NULL) {
                ESP_LOGI(TAG, "Stopping MQTT client");
                esp_mqtt_client_stop(my_mqtt_client);
            }
            esp_restart();

        } else {
            ESP_LOGE(TAG, "OTA update failed: %s", esp_err_to_name(ota_finish_err));
        }
    } else {
        ESP_LOGE(TAG, "Complete data was not received.");
    }

    vTaskDelete(NULL);
}
