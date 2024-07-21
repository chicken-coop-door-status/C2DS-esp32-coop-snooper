#include <stdio.h>
#include <string.h>
#include <time.h>
#include <sys/time.h>
#include "esp_sntp.h"
#include "esp_log.h"
#include "time_sync.h"
#include "esp_timer.h"
#include "esp_sleep.h"

#define TAG "SNTP"

// Callback function for time synchronization
void time_sync_notification_cb(struct timeval *tv) {
    ESP_LOGI(TAG, "Time synchronized");
}

// Function to obtain time from an NTP server and synchronize it with the system time
void initialize_sntp(void) {
    ESP_LOGI(TAG, "Initializing SNTP");

    // Set the NTP server
    esp_sntp_setservername(0, "pool.ntp.org");
    esp_sntp_set_time_sync_notification_cb(time_sync_notification_cb);

    // Set the SNTP operating mode
    esp_sntp_set_sync_mode(SNTP_SYNC_MODE_IMMED);

    // Initialize the SNTP service
    esp_sntp_init();
}

// Function to set the timezone
void set_timezone(const char* timezone) {
    setenv("TZ", timezone, 1);
    tzset();
    ESP_LOGI(TAG, "Timezone set to %s", timezone);
}

// Function to synchronize system time
void synchronize_time(void) {
    // Initialize SNTP
    initialize_sntp();

    // Set the timezone to US Central Time with DST handling
    set_timezone("CST6CDT,M3.2.0,M11.1.0");

    // Wait for time to be synchronized
    time_t now = 0;
    struct tm timeinfo = { 0 };
    int retry = 0;
    const int retry_count = 10;

    while (timeinfo.tm_year < (2016 - 1900) && ++retry < retry_count) {
        ESP_LOGI(TAG, "Waiting for system time to be set... (%d/%d)", retry, retry_count);
        vTaskDelay(2000 / portTICK_PERIOD_MS);
        time(&now);
        localtime_r(&now, &timeinfo);
    }

    if (timeinfo.tm_year < (2016 - 1900)) {
        ESP_LOGE(TAG, "Failed to synchronize time");
    } else {
        ESP_LOGI(TAG, "Time synchronized: %s", asctime(&timeinfo));
    }
}
