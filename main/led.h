#ifndef LED_H
#define LED_H

#include <stdint.h>

void init_led_pwm(void);
void set_led_color(uint32_t red, uint32_t green, uint32_t blue);

#endif // LED_H
