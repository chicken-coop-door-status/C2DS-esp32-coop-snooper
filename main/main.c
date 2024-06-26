#include <stdio.h>
#include <stdint.h>
#include <stddef.h>
#include <string.h>
#include <inttypes.h> // Include this header for PRIu32
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/event_groups.h"
#include "freertos/timers.h"
#include "esp_wifi.h"
#include "esp_event.h"
#include "esp_log.h"
#include "nvs_flash.h"
#include "mqtt_client.h"
#include "esp_system.h"
#include "esp_netif.h"
#include "mbedtls/debug.h"
#include "mbedtls/platform.h"
#include "mbedtls/ssl.h"
#include "cJSON.h"
#include "driver/gpio.h"
#include "driver/ledc.h"

#include "main.h"

// LED GPIOs
#define RED_PIN GPIO_NUM_1
#define GREEN_PIN GPIO_NUM_2
#define BLUE_PIN GPIO_NUM_3

// Certificate paths
extern const uint8_t _binary_root_CA_crt_start[] asm("_binary_root_CA_crt_start");
extern const uint8_t _binary_root_CA_crt_end[] asm("_binary_root_CA_crt_end");
extern const uint8_t _binary_coop_snooper_cert_pem_start[] asm("_binary_coop_snooper_cert_pem_start");
extern const uint8_t _binary_coop_snooper_cert_pem_end[] asm("_binary_coop_snooper_cert_pem_end");
extern const uint8_t _binary_coop_snooper_private_key_start[] asm("_binary_coop_snooper_private_key_start");
extern const uint8_t _binary_coop_snooper_private_key_end[] asm("_binary_coop_snooper_private_key_end");

static const char *TAG = "MQTT_EXAMPLE";
static bool mqtt_message_received = false;
static bool mqtt_setup_complete = false;

// Network timeout in milliseconds
#define NETWORK_TIMEOUT_MS 5000

// Forward declaration of mqtt_app_start
static void mqtt_app_start(void);

static void tls_debug_callback(void *ctx, int level, const char *file, int line, const char *str)
{
    const char *MBEDTLS_DEBUG_LEVEL[] = {"Error", "Warning", "Info", "Debug", "Verbose"};
    ESP_LOGI("mbedTLS", "%s: %s:%04d: %s", MBEDTLS_DEBUG_LEVEL[level], file, line, str);
}

static void event_handler(void *arg, esp_event_base_t event_base, int32_t event_id, void *event_data)
{
    if (event_base == WIFI_EVENT && event_id == WIFI_EVENT_STA_START) {
        esp_wifi_connect();
    } else if (event_base == WIFI_EVENT && event_id == WIFI_EVENT_STA_DISCONNECTED) {
        esp_wifi_connect();
        ESP_LOGI(TAG, "retry to connect to the AP");
    } else if (event_base == IP_EVENT && event_id == IP_EVENT_STA_GOT_IP) {
        ip_event_got_ip_t *event = (ip_event_got_ip_t *)event_data;
        char ip_str[16];
        esp_ip4addr_ntoa(&event->ip_info.ip, ip_str, sizeof(ip_str));
        ESP_LOGI(TAG, "got ip: %s", ip_str);
        mqtt_app_start(); // Start MQTT connection after getting IP
    }
}

static void wifi_init_sta(void)
{
    ESP_ERROR_CHECK(esp_netif_init());
    ESP_ERROR_CHECK(esp_event_loop_create_default());
    esp_netif_create_default_wifi_sta();
    wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
    ESP_ERROR_CHECK(esp_wifi_init(&cfg));
    esp_event_handler_instance_t instance_any_id;
    esp_event_handler_instance_t instance_got_ip;
    ESP_ERROR_CHECK(esp_event_handler_instance_register(WIFI_EVENT,
                                                        ESP_EVENT_ANY_ID,
                                                        &event_handler,
                                                        NULL,
                                                        &instance_any_id));
    ESP_ERROR_CHECK(esp_event_handler_instance_register(IP_EVENT,
                                                        IP_EVENT_STA_GOT_IP,
                                                        &event_handler,
                                                        NULL,
                                                        &instance_got_ip));

    wifi_config_t wifi_config = {
        .sta = {
            .ssid = WIFI_SSID,
            .password = WIFI_PASS}};
    ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_STA));
    ESP_ERROR_CHECK(esp_wifi_set_config(ESP_IF_WIFI_STA, &wifi_config));
    ESP_ERROR_CHECK(esp_wifi_start());

    ESP_LOGI(TAG, "wifi_init_sta finished.");
    ESP_LOGI(TAG, "connect to ap SSID:%s password:%s",
             WIFI_SSID, WIFI_PASS);
}

// Function to initialize LED PWM
void init_led_pwm(void)
{
    ledc_timer_config_t ledc_timer = {
        .speed_mode       = LEDC_LOW_SPEED_MODE,
        .timer_num        = LEDC_TIMER_0,
        .duty_resolution  = LEDC_TIMER_13_BIT,
        .freq_hz          = 5000,
        .clk_cfg          = LEDC_AUTO_CLK
    };
    ESP_ERROR_CHECK(ledc_timer_config(&ledc_timer));

    ledc_channel_config_t ledc_channel = {
        .channel    = LEDC_CHANNEL_0,
        .duty       = 0,
        .gpio_num   = RED_PIN,
        .speed_mode = LEDC_LOW_SPEED_MODE,
        .hpoint     = 0,
        .timer_sel  = LEDC_TIMER_0
    };
    ESP_ERROR_CHECK(ledc_channel_config(&ledc_channel));

    ledc_channel.channel    = LEDC_CHANNEL_1;
    ledc_channel.gpio_num   = GREEN_PIN;
    ESP_ERROR_CHECK(ledc_channel_config(&ledc_channel));

    ledc_channel.channel    = LEDC_CHANNEL_2;
    ledc_channel.gpio_num   = BLUE_PIN;
    ESP_ERROR_CHECK(ledc_channel_config(&ledc_channel));
}

// Function to set LED color with PWM
void set_led_color(uint32_t red, uint32_t green, uint32_t blue) {
    ESP_LOGI(TAG, "Setting LED colors - RED: %" PRIu32 ", GREEN: %" PRIu32 ", BLUE: %" PRIu32, red, green, blue);
    ESP_ERROR_CHECK(ledc_set_duty(LEDC_LOW_SPEED_MODE, LEDC_CHANNEL_0, red));
    ESP_ERROR_CHECK(ledc_update_duty(LEDC_LOW_SPEED_MODE, LEDC_CHANNEL_0));

    ESP_ERROR_CHECK(ledc_set_duty(LEDC_LOW_SPEED_MODE, LEDC_CHANNEL_1, green));
    ESP_ERROR_CHECK(ledc_update_duty(LEDC_LOW_SPEED_MODE, LEDC_CHANNEL_1));

    ESP_ERROR_CHECK(ledc_set_duty(LEDC_LOW_SPEED_MODE, LEDC_CHANNEL_2, blue));
    ESP_ERROR_CHECK(ledc_update_duty(LEDC_LOW_SPEED_MODE, LEDC_CHANNEL_2));
}

// Task to pulse LED
void pulse_led_task(void *pvParameter) {
    int duty = 0;
    int direction = 1;
    uint32_t red = 0, green = 0, blue = 0;
    bool is_white = (bool)pvParameter;

    while (!mqtt_setup_complete || (mqtt_setup_complete && is_white)) {
        duty += direction * 128;
        if (duty >= 8192) {
            duty = 8192;
            direction = -1;
        } else if (duty <= 0) {
            duty = 0;
            direction = 1;
        }

        if (!mqtt_setup_complete) {
            red = green = blue = duty; // Pulse white
        } else if (is_white) {
            blue = duty; // Pulse blue on error
        }

        set_led_color(red, green, blue);
        vTaskDelay(pdMS_TO_TICKS(30));
    }

    // Turn off the pulsing once setup is complete and no error
    set_led_color(0, 0, 0);
    vTaskDelete(NULL);
}

static void mqtt_event_handler(void *handler_args, esp_event_base_t base, int32_t event_id, void *event_data)
{
    esp_mqtt_event_handle_t event = event_data;
    esp_mqtt_client_handle_t client = event->client;
    int msg_id;

    switch (event->event_id)
    {
    case MQTT_EVENT_CONNECTED:
        ESP_LOGI(TAG, "MQTT_EVENT_CONNECTED");
        msg_id = esp_mqtt_client_subscribe(client, MQTT_TOPIC, 0);
        ESP_LOGI(TAG, "sent subscribe successful, msg_id=%d", msg_id);
        mqtt_setup_complete = true; // MQTT setup is complete
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
        printf("TOPIC=%.*s\r\n", event->topic_len, event->topic);
        printf("DATA=%.*s\r\n", event->data_len, event->data);

        // Check if the message is a JSON with a "message" field
        cJSON *json = cJSON_Parse(event->data);
        if (json == NULL) {
            ESP_LOGE(TAG, "Failed to parse JSON");
        } else {
            cJSON *message = cJSON_GetObjectItem(json, "message");
            if (cJSON_IsString(message)) {
                ESP_LOGI(TAG, "Parsed message: %s", message->valuestring);
                mqtt_message_received = true;
                if (strcmp(message->valuestring, "open") == 0) {
                    // Set LED to RED
                    ESP_LOGI(TAG, "Setting LED to RED");
                    set_led_color(8192, 0, 0);
                } else if (strcmp(message->valuestring, "closed") == 0) {
                    // Set LED to GREEN
                    ESP_LOGI(TAG, "Setting LED to GREEN");
                    set_led_color(0, 8192, 0);
                } else if (strcmp(message->valuestring, "error") == 0) {
                    // Start pulsing BLUE
                    ESP_LOGI(TAG, "Setting LED to pulse BLUE");
                    xTaskCreate(&pulse_led_task, "pulse_led_task", 2048, (void*)true, 5, NULL);
                } else {
                    ESP_LOGI(TAG, "Received unknown message: %s", message->valuestring);
                }
            } else {
                ESP_LOGE(TAG, "JSON message item is not a string");
            }
            cJSON_Delete(json);
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

static void mqtt_app_start(void)
{
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
    do {
        err = esp_mqtt_client_start(client);
        if (err != ESP_OK) {
            ESP_LOGE(TAG, "Failed to start MQTT client, retrying in 5 seconds...");
            vTaskDelay(pdMS_TO_TICKS(5000)); // Delay for 5 seconds
        }
    } while (err != ESP_OK);
}

void app_main(void)
{
    mbedtls_debug_set_threshold(4); // Set to the highest debug level
    
    // Initialize the mbedtls SSL configuration
    mbedtls_ssl_config conf;
    mbedtls_ssl_config_init(&conf);
    mbedtls_ssl_conf_dbg(&conf, tls_debug_callback, NULL);
    
    esp_err_t ret = nvs_flash_init();
    if (ret == ESP_ERR_NVS_NO_FREE_PAGES || ESP_ERR_NVS_NEW_VERSION_FOUND) {
        ESP_ERROR_CHECK(nvs_flash_erase());
        ret = nvs_flash_init();
    }
    ESP_ERROR_CHECK(ret);
    ESP_LOGI(TAG, "ESP_WIFI_MODE_STA");
    wifi_init_sta();

    // Initialize LED PWM
    init_led_pwm();

    // Create a task to pulse the LED in white
    xTaskCreate(&pulse_led_task, "pulse_led_task", 2048, (void*)false, 5, NULL);
}
