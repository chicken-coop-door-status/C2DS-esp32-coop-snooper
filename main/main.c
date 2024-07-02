#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_log.h"
#include "nvs_flash.h"
#include "mbedtls/debug.h"
#include "main.h"
#include "wifi.h"
#include "mqtt.h"
#include "led.h"
#include "state_handler.h"
#include "mbedtls/debug.h"  // Add this to include mbedtls debug functions
#include "mp3.h"  // Include the mp3 header

static const char *TAG = "COOP_SNOOPER";


static void tls_debug_callback(void *ctx, int level, const char *file, int line, const char *str)
{
    // Uncomment to enable verbose debugging
    // const char *MBEDTLS_DEBUG_LEVEL[] = {"Error", "Warning", "Info", "Debug", "Verbose"};
    // ESP_LOGI("mbedTLS", "%s: %s:%04d: %s", MBEDTLS_DEBUG_LEVEL[level], file, line, str);
}

void app_main(void)
{
    ESP_LOGI(TAG, "Initializing LED PWM");
    init_led_pwm();

    ESP_LOGI(TAG, "Setting LED state to PULSATING_WHITE");
    current_led_state = LED_PULSATING_WHITE;  

    ESP_LOGI(TAG, "Creating LED task");
    xTaskCreate(&led_task, "led_task", 4096, NULL, 5, NULL);

    // Set mbedtls debug threshold to 0 to disable verbose debugging
    mbedtls_debug_set_threshold(0); 
    
    // Initialize the mbedtls SSL configuration
    mbedtls_ssl_config conf;
    mbedtls_ssl_config_init(&conf);
    mbedtls_ssl_conf_dbg(&conf, tls_debug_callback, NULL);
    
    esp_err_t ret = nvs_flash_init();
    if (ret == ESP_ERR_NVS_NO_FREE_PAGES || ret == ESP_ERR_NVS_NEW_VERSION_FOUND) {
        ESP_ERROR_CHECK(nvs_flash_erase());
        ret = nvs_flash_init();
    }
    ESP_ERROR_CHECK(ret);

    // Initialize WiFi
    wifi_init_sta();

    i2s_std_config_t std_cfg = BSP_I2S_DUPLEX_MONO_CFG(44100);
    ESP_ERROR_CHECK(bsp_audio_init(&std_cfg, &i2s_tx_chan, &i2s_rx_chan));
    xTaskCreate(audio_player_task, "audio_player_task", 8192, NULL, 5, NULL);

    // Initialize MQTT
    ESP_LOGI(TAG, "Initializing MQTT");
    mqtt_app_start();

    // Infinite loop to prevent exiting app_main
    while (true) {
        vTaskDelay(pdMS_TO_TICKS(1000)); // Delay to allow other tasks to run
    }
}
