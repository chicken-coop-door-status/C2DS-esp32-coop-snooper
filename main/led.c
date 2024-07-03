#include "led.h"
#include "driver/ledc.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_log.h"

#define RED_PIN GPIO_NUM_14
#define GREEN_PIN GPIO_NUM_12
#define BLUE_PIN GPIO_NUM_27

static const char *TAG = "LED";

volatile led_state_t current_led_state = LED_OFF;  // Define the variable

void init_led_pwm(void)
{
    ESP_LOGI(TAG, "Initializing LED PWM...");

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

void set_led_color(uint32_t red, uint32_t green, uint32_t blue)
{
    ledc_set_duty(LEDC_LOW_SPEED_MODE, LEDC_CHANNEL_0, red);
    ledc_update_duty(LEDC_LOW_SPEED_MODE, LEDC_CHANNEL_0);

    ledc_set_duty(LEDC_LOW_SPEED_MODE, LEDC_CHANNEL_1, green);
    ledc_update_duty(LEDC_LOW_SPEED_MODE, LEDC_CHANNEL_1);

    ledc_set_duty(LEDC_LOW_SPEED_MODE, LEDC_CHANNEL_2, blue);
    ledc_update_duty(LEDC_LOW_SPEED_MODE, LEDC_CHANNEL_2);
}

void flash_led_color(uint32_t red, uint32_t green, uint32_t blue) {
    set_led_color(red, green, blue);  // Turn on the LED
    vTaskDelay(pdMS_TO_TICKS(500));   // Wait for 500 ms
    set_led_color(0, 0, 0);           // Turn off the LED
    vTaskDelay(pdMS_TO_TICKS(500));   // Wait for 500 ms
}

void pulsate_led_color(uint32_t red, uint32_t green, uint32_t blue) {
    const int max_duty = 8191;  // Maximum duty cycle for 13-bit resolution
    const int step = 50;        // Step size for duty cycle change
    const int delay = 40;       // Delay in milliseconds for each step

    // Increase brightness
    for (int duty = 0; duty <= max_duty; duty += step) {
        set_led_color((red * duty) / max_duty, (green * duty) / max_duty, (blue * duty) / max_duty);
        vTaskDelay(pdMS_TO_TICKS(delay));
    }

    // Decrease brightness
    for (int duty = max_duty; duty >= 0; duty -= step) {
        set_led_color((red * duty) / max_duty, (green * duty) / max_duty, (blue * duty) / max_duty);
        vTaskDelay(pdMS_TO_TICKS(delay));
    }
}

void led_task(void *pvParameter) {
    while(1) {
        switch (current_led_state) {
            case LED_OFF:
                set_led_color(0, 0, 0);
                break;
            case LED_RED:
                set_led_color(8191, 0, 0);
                break;
            case LED_GREEN:
                set_led_color(0, 8191, 0);
                break;
            case LED_BLUE:
                set_led_color(0, 0, 8191);
                break;
            case LED_FLASHING_RED:
                flash_led_color(8191, 0, 0);  // Flash RED
                break;
            case LED_FLASHING_GREEN:
                flash_led_color(0, 8191, 0);  // Flash GREEN
                break;
            case LED_FLASHING_BLUE:
                flash_led_color(0, 0, 8191);  // Flash BLUE
                break;
            case LED_FLASHING_WHITE:
                flash_led_color(8191, 8191, 8191);  // Flash WHITE
                break;
            case LED_PULSATING_RED:
                pulsate_led_color(8191, 0, 0);
                break;
            case LED_PULSATING_GREEN:
                pulsate_led_color(0, 8191, 0);
                break;
            case LED_PULSATING_BLUE:
                pulsate_led_color(0, 0, 8191);
                break;
            case LED_PULSATING_WHITE:
                pulsate_led_color(8191, 8191, 8191);
                break;
            default:
                set_led_color(0, 0, 0);  // Ensure LED is off
                break;
        }
        vTaskDelay(pdMS_TO_TICKS(100)); // Small delay to avoid CPU overuse
    }
}
