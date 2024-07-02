#ifndef SPIFFS_H
#define SPIFFS_H

#include "esp_err.h"
#include <stdint.h>
#include <stddef.h>

// Initialize SPIFFS
esp_err_t init_spiffs(void);

// Read MP3 file from SPIFFS
uint8_t* read_mp3_file(const char *path, size_t *size);

#endif // SPIFFS_H
