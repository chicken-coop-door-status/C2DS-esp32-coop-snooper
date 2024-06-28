#ifndef LED_H
#define LED_H

#include <stdint.h>
typedef enum {
    LED_OFF,
    LED_RED,
    LED_GREEN,
    LED_BLUE,
    LED_FLASHING_RED,
    LED_FLASHING_GREEN,
    LED_FLASHING_BLUE,
    LED_FLASHING_WHITE,
    LED_PULSATING_RED,
    LED_PULSATING_GREEN,
    LED_PULSATING_BLUE,
    LED_PULSATING_WHITE
} led_state_t;

extern volatile led_state_t current_led_state;

void init_led_pwm(void);
void set_led_color(uint32_t red, uint32_t green, uint32_t blue);
void led_task(void *pvParameter); 

#endif // LED_H
