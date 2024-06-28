#include "state_handler.h"
#include <freertos/FreeRTOS.h> // For FreeRTOS related definitions
#include <freertos/task.h> // For task-related functions and definitions
#include <stdarg.h> // For va_list
#include <stdio.h> // For standard input/output functions
#include <string.h> // For string manipulation functions
#include "mbedtls/ssl.h" // For mbedtls functions
#include "esp_log.h"

static const char *TAG = "STATE_HANDLER";

void set_led_color_based_on_state(const char *state) {
    if (strcmp(state, "SENSING_DOOR_OPEN") == 0) {
        current_led_state = LED_RED;
    } else if (strcmp(state, "SENSING_DOOR_CLOSED") == 0) {
        current_led_state = LED_GREEN;
    } else if (strcmp(state, "SENSING_DOOR_CLOSED_ERROR") == 0 ||
               strcmp(state, "SENSING_DOOR_OPEN_ERROR") == 0 ||
               strcmp(state, "MONITOR_SENSING_TIMEOUT") == 0 ||
               strcmp(state, "MONITOR_SENSING_EXCEPTION") == 0 ||
               strcmp(state, "MONITOR_TWILIGHT_TIMEOUT") == 0 ||
               strcmp(state, "MONITOR_TWILIGHT_EXCEPTION") == 0) {
        current_led_state = LED_FLASHING_RED;
    } else if (strcmp(state, "SENSING_DOOR_SENSOR_ERROR") == 0) {
        current_led_state = LED_FLASHING_BLUE;
    } else {
        ESP_LOGI(TAG, "Received unknown state: %s", state);
    }
}
