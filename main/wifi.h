#ifndef WIFI_H
#define WIFI_H

#include "freertos/FreeRTOS.h"
#include "freertos/semphr.h"

#define MAX_RETRY 7 

void wifi_init_sta(void);
extern SemaphoreHandle_t wifi_connected_semaphore;

#endif // WIFI_H
