#ifndef PROV_WIFI_H
#define PROV_WIFI_H

#include <esp_event.h>
#include <esp_log.h>
#include <esp_wifi.h>
#include <freertos/FreeRTOS.h>
#include <freertos/event_groups.h>
#include <freertos/task.h>
#include <nvs_flash.h>
#include <stdio.h>
#include <string.h>
#include <wifi_provisioning/manager.h>
#include <wifi_provisioning/scheme_ble.h>

/* Constants */
#define PROV_TRANSPORT_BLE "ble"

/* Wi-Fi Event Group and Event Bit */
extern const int WIFI_CONNECTED_EVENT;
extern EventGroupHandle_t wifi_event_group;

/* Function prototypes */
void init_provision_wifi(void);
esp_err_t custom_prov_data_handler(uint32_t session_id, const uint8_t *inbuf, ssize_t inlen, uint8_t **outbuf,
                                   ssize_t *outlen, void *priv_data);
static void wifi_init_sta(void);
static void event_handler(void *arg, esp_event_base_t event_base, int32_t event_id, void *event_data);
static void get_device_service_name(char *service_name, size_t max);

#endif /* PROV_WIFI_H */
