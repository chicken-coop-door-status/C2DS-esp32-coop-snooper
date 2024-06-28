#include "mqtt.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_log.h"
#include "mqtt_client.h"
#include "cJSON.h"
#include "main.h"
#include "led.h"
#include "state_handler.h"
#include "mbedtls/debug.h"  // Add this to include mbedtls debug functions

// Declare the global/static variables
bool mqtt_setup_complete = false;
bool mqtt_message_received = false;

// Define NETWORK_TIMEOUT_MS
#define NETWORK_TIMEOUT_MS 10000  // Example value, set as appropriate for your application

// Include binary data
extern const uint8_t _binary_coop_snooper_cert_pem_start[];
extern const uint8_t _binary_coop_snooper_private_key_start[];

static const char *TAG = "MQTT";

static void mqtt_event_handler(void *handler_args, esp_event_base_t base, int32_t event_id, void *event_data)
{
    esp_mqtt_event_handle_t event = event_data;
    esp_mqtt_client_handle_t client = event->client;
    int msg_id;

    switch (event->event_id)
    {
    case MQTT_EVENT_CONNECTED:
        ESP_LOGI(TAG, "MQTT_EVENT_CONNECTED");
        msg_id = esp_mqtt_client_subscribe(client, "coop/status", 0);
        ESP_LOGI(TAG, "sent subscribe successful, msg_id=%d", msg_id);
        mqtt_setup_complete = true; // MQTT setup is complete
        msg_id = esp_mqtt_client_publish(client, "coop/status/request", "request", 0, 0, 0); // Publish status request
        ESP_LOGI(TAG, "sent status request, msg_id=%d", msg_id);
        break;
    case MQTT_EVENT_DISCONNECTED:
        ESP_LOGI(TAG, "MQTT_EVENT_DISCONNECTED");
        break;
    case MQTT_EVENT_SUBSCRIBED:
        ESP_LOGI(TAG, "MQTT_EVENT_SUBSCRIBED, msg_id=%d", event->msg_id);
        break;
    case MQTT_EVENT_UNSUBSCRIBED:
        ESP_LOGI(TAG, "MQTT_EVENT_UNSUBSCRIBED, msg_id=%d", event->msg_id);
        break;
    case MQTT_EVENT_PUBLISHED:
        ESP_LOGI(TAG, "MQTT_EVENT_PUBLISHED, msg_id=%d", event->msg_id);
        break;
    case MQTT_EVENT_DATA:
        ESP_LOGI(TAG, "MQTT_EVENT_DATA");
        ESP_LOGI(TAG,"TOPIC=%.*s\r", event->topic_len, event->topic);
        ESP_LOGI(TAG,"DATA=%.*s\r", event->data_len, event->data);

        if (strncmp(event->topic, "coop/status", event->topic_len) == 0) {
            ESP_LOGW(TAG, "Received coop/status");

            // Handle the status response
            cJSON *json = cJSON_Parse(event->data);
            if (json == NULL) {
                ESP_LOGE(TAG, "Failed to parse JSON");
            } else {
                cJSON *state = cJSON_GetObjectItem(json, "state");
                if (cJSON_IsString(state)) {
                    ESP_LOGI(TAG, "Parsed state: %s", state->valuestring);
                    set_led_color_based_on_state(state->valuestring);
                } else {
                    ESP_LOGE(TAG, "JSON state item is not a string");
                }
                cJSON_Delete(json);
            }
        } 
        break;
    case MQTT_EVENT_ERROR:
        ESP_LOGI(TAG, "MQTT_EVENT_ERROR");
        if (event->error_handle->error_type == MQTT_ERROR_TYPE_ESP_TLS) {
            ESP_LOGI(TAG, "Last ESP error code: 0x%x", event->error_handle->esp_tls_last_esp_err);
            ESP_LOGI(TAG, "Last TLS stack error code: 0x%x", event->error_handle->esp_tls_stack_err);
            ESP_LOGI(TAG, "Last TLS library error code: 0x%x", event->error_handle->esp_tls_cert_verify_flags);
        } else if (event->error_handle->error_type == MQTT_ERROR_TYPE_CONNECTION_REFUSED) {
            ESP_LOGI(TAG, "Connection refused error: 0x%x", event->error_handle->connect_return_code);
        } else {
            ESP_LOGI(TAG, "Unknown error type: 0x%x", event->error_handle->error_type);
        }
        break;
    default:
        ESP_LOGI(TAG, "Other event id:%d", event->event_id);
        break;
    }
}

void mqtt_app_start(void)
{
    // Set mbedtls debug threshold to a higher level for detailed logs
    mbedtls_debug_set_threshold(0);

    const esp_mqtt_client_config_t mqtt_cfg = {
        .broker = {
            .address = {
                .uri = AWS_IOT_ENDPOINT,
            },
        },
        .credentials = {
            .authentication = {
                .certificate = (const char *)_binary_coop_snooper_cert_pem_start,
                .key = (const char *)_binary_coop_snooper_private_key_start,
            },
        },
        .network = {
            .timeout_ms = NETWORK_TIMEOUT_MS,
        },
        .session = {
            .keepalive = 120,
        },
        .buffer = {
            .size = 4096,
            .out_size = 4096,
        },
        .task = {
            .priority = 5,
        },
    };

    esp_mqtt_client_handle_t client = esp_mqtt_client_init(&mqtt_cfg);
    esp_mqtt_client_register_event(client, ESP_EVENT_ANY_ID, mqtt_event_handler, client);

    esp_err_t err;
    int retry_count = 0;
    const int max_retries = 5;
    const int retry_delay_ms = 5000;

    do {
        err = esp_mqtt_client_start(client);
        if (err != ESP_OK) {
            ESP_LOGE(TAG, "Failed to start MQTT client, retrying in %d seconds... (%d/%d)", retry_delay_ms / 1000, retry_count + 1, max_retries);
            vTaskDelay(pdMS_TO_TICKS(retry_delay_ms)); // Delay for 5 seconds
            retry_count++;
        }
    } while (err != ESP_OK && retry_count < max_retries);

    if (err != ESP_OK) {
        ESP_LOGE(TAG, "Failed to start MQTT client after %d retries", retry_count);
    }
}
