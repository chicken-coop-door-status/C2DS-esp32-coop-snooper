#ifndef MQTT_H
#define MQTT_H
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_log.h"
#include "mqtt_client.h"
#include "cJSON.h"
#include "led.h"
#include "state_handler.h"
#include "mbedtls/debug.h"  // Add this to include mbedtls debug functions
#include "ota.h"
#include "sdkconfig.h"

#ifdef TENNIS_HOUSE
extern const uint8_t coop_snooper_tennis_home_certificate_pem[];
extern const uint8_t coop_snooper_tennis_home_private_pem_key[];
#elif defined(FARM_HOUSE)
extern const uint8_t coop_snooper_farmhouse_certificate_pem[];
extern const uint8_t coop_snooper_farmhouse_private_pem_key[];
#endif

extern const uint8_t *cert;
extern const uint8_t *key;

esp_mqtt_client_handle_t mqtt_app_start(void);

#endif // MQTT_H
