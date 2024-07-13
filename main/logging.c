#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/queue.h"
#include "mqtt_client.h"
#include "esp_log.h"
#include "sdkconfig.h"
#include <stdarg.h>
#include <stdio.h>
#include <string.h>
#include "sdkconfig.h"
#include "cJSON.h"
#include "wifi.h" // Include wifi header to access semaphore

#ifdef TENNIS_HOUSE
static const char *MQTT_HOST = "TENNIS_HOUSE";
#elif defined(FARM_HOUSE)
static const char *MQTT_HOST = "FARM_HOUSE";
#endif

#define LOG_QUEUE_SIZE 10
#define LOG_MESSAGE_MAX_LENGTH 1024

static const char *TAG = "LOGGING";
esp_mqtt_client_handle_t mqtt_client;
static QueueHandle_t log_queue;

typedef struct {
    char message[LOG_MESSAGE_MAX_LENGTH];
} log_message_t;

static int mqtt_vprintf(const char *fmt, va_list args) {
    static char log_buffer[LOG_MESSAGE_MAX_LENGTH];
    int len = vsnprintf(log_buffer, sizeof(log_buffer), fmt, args);

    if (len > 0) {
        log_message_t log_message;
        strncpy(log_message.message, log_buffer, LOG_MESSAGE_MAX_LENGTH - 1);
        log_message.message[LOG_MESSAGE_MAX_LENGTH - 1] = '\0';
        if (xQueueSend(log_queue, &log_message, portMAX_DELAY) != pdPASS) {
            ESP_LOGE(TAG, "Failed to send log message to queue");
        }
    }

    return vprintf(fmt, args); // Also print to the default UART
}

void logging_task(void *pvParameters) {
    log_message_t log_message;
    log_queue = xQueueCreate(LOG_QUEUE_SIZE, sizeof(log_message_t));
    if (log_queue == NULL) {
        ESP_LOGE(TAG, "Failed to create log queue");
        return;
    }
    esp_log_set_vprintf(mqtt_vprintf);
    ESP_LOGI(TAG, "Custom logging initialized. Host: %s Topic: %s", MQTT_HOST, CONFIG_MQTT_PUBLISH_LOGGING_TOPIC);

    mqtt_client = (esp_mqtt_client_handle_t)pvParameters;
    while (true) {
        if (xQueueReceive(log_queue, &log_message, portMAX_DELAY) == pdPASS 
            && mqtt_client != NULL 
            && xSemaphoreTake(wifi_connected_semaphore, 0) == pdTRUE) { 
                cJSON *root = cJSON_CreateObject();
                cJSON_AddStringToObject(root, MQTT_HOST, log_message.message);
                const char *json_string = cJSON_Print(root);
                esp_mqtt_client_publish(mqtt_client, CONFIG_MQTT_PUBLISH_LOGGING_TOPIC, json_string, 0, 1, 0);
                cJSON_Delete(root);
        }
    }
}
