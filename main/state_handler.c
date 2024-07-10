#include "state_handler.h"
#include <freertos/FreeRTOS.h> // For FreeRTOS related definitions
#include <freertos/task.h> // For task-related functions and definitions
#include <stdarg.h> // For va_list
#include <stdio.h> // For standard input/output functions
#include <string.h> // For string manipulation functions
#include "mbedtls/ssl.h" // For mbedtls functions
#include "esp_log.h"
#include "mp3.h"

static const char *TAG = "STATE_HANDLER";

// Function to map string to enum
led_state_t lookup_led_state(const char* str) {
    if (strcmp(str, "LED_OFF") == 0) return LED_OFF;
    if (strcmp(str, "LED_RED") == 0) return LED_RED;
    if (strcmp(str, "LED_GREEN") == 0) return LED_GREEN;
    if (strcmp(str, "LED_BLUE") == 0) return LED_BLUE;
    if (strcmp(str, "LED_FLASHING_RED") == 0) return LED_FLASHING_RED;
    if (strcmp(str, "LED_FLASHING_GREEN") == 0) return LED_FLASHING_GREEN;
    if (strcmp(str, "LED_FLASHING_BLUE") == 0) return LED_FLASHING_BLUE;
    if (strcmp(str, "LED_FLASHING_WHITE") == 0) return LED_FLASHING_WHITE;
    if (strcmp(str, "LED_PULSATING_RED") == 0) return LED_PULSATING_RED;
    if (strcmp(str, "LED_PULSATING_GREEN") == 0) return LED_PULSATING_GREEN;
    if (strcmp(str, "LED_PULSATING_BLUE") == 0) return LED_PULSATING_BLUE;
    if (strcmp(str, "LED_PULSATING_WHITE") == 0) return LED_PULSATING_WHITE;
    
    // Default case if no match found
    return LED_OFF;
}

void set_led_color_based_on_state(const char *state) {

    current_led_state = lookup_led_state(state);
    
    ESP_LOGI(TAG, "Setting LED color based on state: %s", state);

    if (strcmp(state,"LED_GREEN") != 0) {
        set_audio_playback(true);
        set_volume(1.0f);
        set_gain(true);
        enable_amplifier(true);
    }
}