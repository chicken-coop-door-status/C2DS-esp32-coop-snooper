#ifndef STATE_HANDLER_H
#define STATE_HANDLER_H

#include <stdint.h>
#include "led.h"  // Include the led header

void set_led_color_based_on_state(const char *state);
extern void set_led_color(uint32_t red, uint32_t green, uint32_t blue);

#endif // STATE_HANDLER_H
