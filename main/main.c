#include "cJSON.h"
#include "esp_log.h"
#include "esp_task_wdt.h"
#include "freertos/FreeRTOS.h"
#include "freertos/semphr.h"
#include "freertos/task.h"
#include "freertos/timers.h" // Include FreeRTOS timers header
#include "gecl-misc-util-manager.h"
#include "gecl-mqtt-manager.h"
#include "gecl-nvs-manager.h"
#include "gecl-ota-manager.h"
#include "gecl-rgb-led-manager.h"
#include "gecl-time-sync-manager.h"
#include "gecl-versioning-manager.h"
#include "gecl-wifi-manager.h"
#include "mbedtls/debug.h" // Add this to include mbedtls debug functions
#include "mp3.h"           // Include the mp3 header
#include "nvs_flash.h"

#define ORPHAN_TIMEOUT pdMS_TO_TICKS(7200000) // 2 hours in milliseconds

static const char *TAG = "COOP_SNOOPER";
const char *device_name = CONFIG_WIFI_HOSTNAME;

SemaphoreHandle_t audioSemaphore;  // Add semaphore handle for audio playback
SemaphoreHandle_t timer_semaphore; // Add semaphore handle timer for audio playback

TaskHandle_t ota_task_handle = NULL; // Task handle for OTA updating
TimerHandle_t orphan_timer = NULL;

#ifdef TENNIS_HOUSE
extern const uint8_t coop_snooper_tennis_home_certificate_pem[];
extern const uint8_t coop_snooper_tennis_home_private_pem_key[];
#elif defined(FARM_HOUSE)
extern const uint8_t coop_snooper_farmhouse_certificate_pem[];
extern const uint8_t coop_snooper_farmhouse_private_pem_key[];
#elif defined(TEST)
extern const uint8_t coop_snooper_test_certificate_pem[];
extern const uint8_t coop_snooper_test_private_pem_key[];
#endif

void squawk(void)
{
    set_volume(1.0f);         // Set volume first
    set_gain(true);           // Set gain
    enable_amplifier(true);   // Enable the amplifier
    set_audio_playback(true); // Start playback after everything is configured
}

// Callback function for timer expiration
void orphan_timer_callback(TimerHandle_t xTimer)
{
    ESP_LOGE(TAG, "No message received for 2 hours. Triggering notification.");

    // Set LED to cyan and trigger squawk
    set_rgb_led_named_color("LED_BLINK_CYAN");
    squawk();
}

// Function to reset the timer whenever a message is received
void reset_orphan_timer(void)
{
    if (xTimerReset(orphan_timer, 0) != pdPASS)
    {
        ESP_LOGE(TAG, "Orphan timer failed to reset");
    }
    else
    {
        ESP_LOGI(TAG, "Orphan timer reset successfully");
    }
}

void custom_handle_mqtt_event_connected(esp_mqtt_event_handle_t event)
{
    esp_mqtt_client_handle_t client = event->client;
    ESP_LOGI(TAG, "Custom handler: MQTT_EVENT_CONNECTED");
    int msg_id;

    msg_id = esp_mqtt_client_subscribe(client, CONFIG_MQTT_SUBSCRIBE_STATUS_TOPIC, 0);
    ESP_LOGI(TAG, "Subscribed to topic %s, msg_id=%d", CONFIG_MQTT_SUBSCRIBE_STATUS_TOPIC, msg_id);

    msg_id = esp_mqtt_client_subscribe(client, CONFIG_MQTT_SUBSCRIBE_OTA_UPDATE_SNOOPER_TOPIC, 0);
    ESP_LOGI(TAG, "Subscribed to topic %s, msg_id=%d", CONFIG_MQTT_SUBSCRIBE_OTA_UPDATE_SNOOPER_TOPIC, msg_id);

    msg_id =
        esp_mqtt_client_publish(client, CONFIG_MQTT_PUBLISH_STATUS_TOPIC, "{\"message\":\"status_request\"}", 0, 0, 0);
    ESP_LOGI(TAG, "Published initial status request, msg_id=%d", msg_id);
}

void custom_handle_mqtt_event_disconnected(esp_mqtt_event_handle_t event)
{
    ESP_LOGI(TAG, "Custom handler: MQTT_EVENT_DISCONNECTED");
    if (ota_task_handle != NULL)
    {
        vTaskDelete(ota_task_handle);
        ota_task_handle = NULL;
    }
    // Reconnect logic
    int retry_count = 0;
    const int max_retries = 5;
    const int retry_delay_ms = 5000;
    esp_err_t err;
    esp_mqtt_client_handle_t client = event->client;

    // Check if the network is connected before attempting reconnection
    if (wifi_active())
    {
        do
        {
            ESP_LOGI(TAG, "Attempting to reconnect, retry %d/%d", retry_count + 1, max_retries);
            err = esp_mqtt_client_reconnect(client);
            if (err != ESP_OK)
            {
                ESP_LOGE(TAG, "Failed to reconnect MQTT client, retrying in %d seconds...", retry_delay_ms / 1000);
                vTaskDelay(pdMS_TO_TICKS(retry_delay_ms)); // Delay for 5 seconds
                retry_count++;
            }
        } while (err != ESP_OK && retry_count < max_retries);

        if (err != ESP_OK)
        {
            ESP_LOGE(TAG, "Failed to reconnect MQTT client after %d retries. Restarting", retry_count);
            esp_restart();
        }
    }
    else
    {
        ESP_LOGE(TAG, "Network not connected, skipping MQTT reconnection");
        esp_restart();
    }
}

void custom_handle_mqtt_event_subscribe(esp_mqtt_event_handle_t event)
{
    // Handle the status response
    cJSON *json = cJSON_Parse(event->data);
    if (json == NULL)
    {
        ESP_LOGE(TAG, "Failed to parse JSON");
    }
    else
    {
        cJSON *state = cJSON_GetObjectItem(json, "LED");
        const char *led_state = cJSON_GetStringValue(state);
        assert(led_state != NULL);
        set_rgb_led_named_color(led_state);

        // Check if the led_state does not contain "GREEN" but contains "BLINK"
        if (strstr(led_state, "GREEN") == NULL && strstr(led_state, "BLINK") != NULL)
        {
            squawk(); // Call squawk if the condition is met
        }
        cJSON_Delete(json);
    }
}

bool extract_ota_url_from_event(esp_mqtt_event_handle_t event, char *ota_url)
{
    bool success = false;
    char mac_address[18];
    cJSON *root = cJSON_Parse(event->data);

    get_burned_in_mac_address(mac_address);
    ESP_LOGI(TAG, "Burned-In MAC Address: %s\n", mac_address);

    cJSON *host_key = cJSON_GetObjectItem(root, mac_address);
    const char *host_key_value = cJSON_GetStringValue(host_key);

    if (!host_key || !host_key_value)
    {
        ESP_LOGE(TAG, "Invalid or missing '%s' key in JSON", mac_address);
    }
    else
    {
        size_t url_len = strlen(host_key_value);
        strncpy(ota_url, host_key_value, url_len);
        ota_url[url_len] = '\0'; // Manually set the null terminator
        success = true;
    }

    cJSON_Delete(root); // Free JSON object
    return success;
}

void custom_handle_mqtt_event_ota(esp_mqtt_event_handle_t event)
{
    if (ota_task_handle != NULL)
    {
        eTaskState task_state = eTaskGetState(ota_task_handle);
        if (task_state != eDeleted)
        {
            char log_message[256]; // Adjust the size according to your needs
            snprintf(log_message, sizeof(log_message),
                     "OTA task is already running or not yet cleaned up, skipping OTA update. task_state=%d",
                     task_state);

            ESP_LOGW(TAG, "%s", log_message);
            return;
        }
        else
        {
            // Clean up task handle if it has been deleted
            ota_task_handle = NULL;
        }
    }

    // Parse the message and get any URL associated with our MAC address
    assert(event->data != NULL);
    assert(event->data_len > 0);

    char ota_url[512];

    if (!extract_ota_url_from_event(event, ota_url))
    {
        ESP_LOGE(TAG, "Failed to extract OTA URL from event data");
        return;
    }

    set_rgb_led_named_color("LED_BLINK_GREEN");

    // Pass the allocated URL string to the OTA task
    if (xTaskCreate(&ota_task, "ota_task", 8192, (void *)ota_url, 5, &ota_task_handle) != pdPASS)
    {
        ESP_LOGE(TAG, "Failed to create OTA task.");
        esp_mqtt_client_handle_t client = event->client;
        // If the above task aborts, ask for status so we reset the LED color
        esp_mqtt_client_publish(client, CONFIG_MQTT_PUBLISH_STATUS_TOPIC, "{\"message\":\"status_request\"}", 0, 0, 0);
    }
}

void custom_handle_mqtt_event_data(esp_mqtt_event_handle_t event)
{

    ESP_LOGW(TAG, "Received topic %.*s", event->topic_len, event->topic);

    // Reset the orphan timer whenever a message is received
    reset_orphan_timer();

    if (strncmp(event->topic, CONFIG_MQTT_SUBSCRIBE_STATUS_TOPIC, event->topic_len) == 0)
    {
        custom_handle_mqtt_event_subscribe(event);
    }
    else if (strncmp(event->topic, CONFIG_MQTT_SUBSCRIBE_OTA_UPDATE_SNOOPER_TOPIC, event->topic_len) == 0)
    {
        custom_handle_mqtt_event_ota(event);
    }
    else
    {
        ESP_LOGW(TAG, "Un-Handled topic %.*s", event->topic_len, event->topic);
    }
}

void custom_handle_mqtt_event_error(esp_mqtt_event_handle_t event)
{
    ESP_LOGI(TAG, "Custom handler: MQTT_EVENT_ERROR");
    if (event->error_handle->error_type == MQTT_ERROR_TYPE_ESP_TLS)
    {
        ESP_LOGI(TAG, "Last ESP error code: 0x%x", event->error_handle->esp_tls_last_esp_err);
        ESP_LOGI(TAG, "Last TLS stack error code: 0x%x", event->error_handle->esp_tls_stack_err);
        ESP_LOGI(TAG, "Last TLS library error code: 0x%x", event->error_handle->esp_tls_cert_verify_flags);
    }
    else if (event->error_handle->error_type == MQTT_ERROR_TYPE_CONNECTION_REFUSED)
    {
        ESP_LOGI(TAG, "Connection refused error: 0x%x", event->error_handle->connect_return_code);
    }
    else
    {
        ESP_LOGI(TAG, "Unknown error type: 0x%x", event->error_handle->error_type);
    }
    esp_restart();
}

void app_main(void)
{
#ifdef TENNIS_HOUSE
    printf("Configuration: TENNIS_HOUSE\n");
    static const char *LOCATION = "Tennis House";
    const uint8_t *cert = coop_snooper_tennis_home_certificate_pem;
    const uint8_t *key = coop_snooper_tennis_home_private_pem_key;
#elif defined(FARM_HOUSE)
    printf("Configuration: FARM_HOUSE\n");
    static const char *LOCATION = "Farm House";
    const uint8_t *cert = coop_snooper_farmhouse_certificate_pem;
    const uint8_t *key = coop_snooper_farmhouse_private_pem_key;
#elif defined(TEST)
    printf("Configuration: TEST\n");
    static const char *LOCATION = "Test";
    const uint8_t *cert = coop_snooper_test_certificate_pem;
    const uint8_t *key = coop_snooper_test_private_pem_key;
#else
    printf("Configuration: UNKNOWN\n");
    return;
#endif

    init_nvs();

    init_wifi();

    init_time_sync();

    mqtt_set_event_connected_handler(custom_handle_mqtt_event_connected);
    mqtt_set_event_disconnected_handler(custom_handle_mqtt_event_disconnected);
    mqtt_set_event_data_handler(custom_handle_mqtt_event_data);
    mqtt_set_event_error_handler(custom_handle_mqtt_event_error);

    mqtt_config_t config = {.certificate = cert, .private_key = key, .broker_uri = CONFIG_AWS_IOT_ENDPOINT};

    esp_mqtt_client_handle_t client = init_mqtt(&config);

    init_rgb_led();

    set_rgb_led_named_color("LED_BLINK_WHITE");

    // Initialize audio semaphore
    audioSemaphore = xSemaphoreCreateBinary();
    if (audioSemaphore == NULL)
    {
        ESP_LOGE(TAG, "Failed to create audio semaphore");
        return;
    }

    timer_semaphore = xSemaphoreCreateBinary();
    if (timer_semaphore == NULL)
    {
        ESP_LOGE(TAG, "Failed to create timer semaphore");
        return;
    }

    xTaskCreate(audio_player_task, "audio_player_task", 8192, NULL, 5, NULL);

    // Create an orphan timer to trigger a notification if no message is received for 2 hours
    orphan_timer = xTimerCreate("orphan_timer", ORPHAN_TIMEOUT, pdFALSE, (void *)0, orphan_timer_callback);

    if (orphan_timer == NULL)
    {
        ESP_LOGE(TAG, "Failed to create notification timer");
        return;
    }

    // Start the timer when the system boots
    if (xTimerStart(orphan_timer, 0) != pdPASS)
    {
        ESP_LOGE(TAG, "Failed to start notification timer");
    }

    // Infinite loop to prevent exiting app_main
    while (true)
    {
        vTaskDelay(pdMS_TO_TICKS(1000)); // Delay to allow other tasks to run
    }
}
