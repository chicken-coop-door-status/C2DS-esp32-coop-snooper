#include "mqtt.h"

static const char *TAG = "MQTT";

TaskHandle_t ota_task_handle = NULL;  // Task handle for OTA updating

// Declare the global/static variables
bool mqtt_setup_complete = false;
bool mqtt_message_received = false;

// Define NETWORK_TIMEOUT_MS
#define NETWORK_TIMEOUT_MS 10000  // Example value, set as appropriate for your application

#ifdef TENNIS_HOUSE
static const char *LOCATION = "Tennis House";
const uint8_t *cert = coop_snooper_tennis_home_certificate_pem;
const uint8_t *key = coop_snooper_tennis_home_private_pem_key;
#elif defined(FARM_HOUSE)
static const char *LOCATION = "Farm House";
const uint8_t *cert = coop_snooper_farmhouse_certificate_pem;
const uint8_t *key = coop_snooper_farmhouse_private_pem_key;
#endif

static void mqtt_event_handler(void *handler_args, esp_event_base_t base, int32_t event_id, void *event_data)
{
    esp_mqtt_event_handle_t event = event_data;
    esp_mqtt_client_handle_t client = event->client;
    int msg_id;

    switch (event->event_id)
    {
    case MQTT_EVENT_CONNECTED:
        ESP_LOGI(TAG, "MQTT_EVENT_CONNECTED");
        ESP_LOGW(TAG, "Location: %s", LOCATION);
        msg_id = esp_mqtt_client_subscribe(client, CONFIG_MQTT_SUBSCRIBE_STATUS_TOPIC, 0);
        msg_id = esp_mqtt_client_subscribe(client, CONFIG_MQTT_SUBSCRIBE_OTA_UPDATE_SNOOPER_TOPIC, 0);
        msg_id = esp_mqtt_client_publish(client, CONFIG_MQTT_PUBLISH_STATUS_TOPIC, "{\"message\":\"status_request\"}", 0, 0, 0);
        mqtt_setup_complete = true; // MQTT setup is complete
        break;
    case MQTT_EVENT_DISCONNECTED:
        ESP_LOGI(TAG, "MQTT_EVENT_DISCONNECTED");
        if (ota_task_handle != NULL) {
            vTaskDelete(ota_task_handle);
            ota_task_handle = NULL;
        }
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

        if (strncmp(event->topic, CONFIG_MQTT_SUBSCRIBE_STATUS_TOPIC, event->topic_len) == 0) {
            ESP_LOGW(TAG, "Received topic %s", CONFIG_MQTT_SUBSCRIBE_STATUS_TOPIC);
            // Handle the status response
            cJSON *json = cJSON_Parse(event->data);
            if (json == NULL) {
                ESP_LOGE(TAG, "Failed to parse JSON");
            } else {
                cJSON *state = cJSON_GetObjectItem(json, "LED");
                if (cJSON_IsString(state)) {
                    ESP_LOGI(TAG, "Parsed state: %s", state->valuestring);
                    set_led_color_based_on_state(state->valuestring);
                } else {
                    ESP_LOGE(TAG, "JSON state item is not a string");
                }
                cJSON_Delete(json);
            }
        } else if (strncmp(event->topic, CONFIG_MQTT_SUBSCRIBE_OTA_UPDATE_SNOOPER_TOPIC, event->topic_len) == 0) {
            ESP_LOGI(TAG, "Received topic %s", CONFIG_MQTT_SUBSCRIBE_OTA_UPDATE_SNOOPER_TOPIC);
            xTaskCreate(&ota_task, "ota_task", 8192, NULL, 5, &ota_task_handle);
        } else {
            ESP_LOGW(TAG, "Received unknown topic");
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
    // mbedtls_debug_set_threshold(0);

    const esp_mqtt_client_config_t mqtt_cfg = {
        .broker = {
            .address = {
                .uri = CONFIG_AWS_IOT_ENDPOINT,
            },
        },
        .credentials = {
            .authentication = {
                .certificate = (const char *)cert,
                .key = (const char *)key,
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
