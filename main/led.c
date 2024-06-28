#include "led.h"
#include "driver/ledc.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_log.h"

#define RED_PIN GPIO_NUM_1
#define GREEN_PIN GPIO_NUM_2
#define BLUE_PIN GPIO_NUM_3

static const char *TAG = "LED";

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
    ESP_LOGI(TAG, "Setting LED colors - RED: %lu, GREEN: %lu, BLUE: %lu", red, green, blue);

    ledc_set_duty(LEDC_LOW_SPEED_MODE, LEDC_CHANNEL_0, red);
    ledc_update_duty(LEDC_LOW_SPEED_MODE, LEDC_CHANNEL_0);

    ledc_set_duty(LEDC_LOW_SPEED_MODE, LEDC_CHANNEL_1, green);
    ledc_update_duty(LEDC_LOW_SPEED_MODE, LEDC_CHANNEL_1);

    ledc_set_duty(LEDC_LOW_SPEED_MODE, LEDC_CHANNEL_2, blue);
    ledc_update_duty(LEDC_LOW_SPEED_MODE, LEDC_CHANNEL_2);

    ESP_LOGI(TAG, "LED color set");
}

