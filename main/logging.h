#ifndef LOGGING_H
#define LOGGING_H

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/queue.h"
#include "mqtt_client.h"
#include "esp_log.h"
#include "sdkconfig.h"
#include <stdarg.h>
#include <stdio.h>
#include <string.h>

#define LOG_QUEUE_SIZE 10
#define LOG_MESSAGE_MAX_LENGTH 256

#ifdef __cplusplus
extern "C" {
#endif

typedef struct {
    char message[LOG_MESSAGE_MAX_LENGTH];
} log_message_t;

extern esp_mqtt_client_handle_t mqtt_client;
extern QueueHandle_t log_queue;

// Function declarations
int mqtt_vprintf(const char *fmt, va_list args);
void logging_task(void *pvParameters);

#ifdef __cplusplus
}
#endif

#endif // LOGGING_H
