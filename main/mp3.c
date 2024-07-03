#include "string.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/semphr.h"
#include "esp_log.h"
#include "driver/gpio.h"
#include "driver/dac.h"
#include "mp3.h"
#include "mp3dec.h"
#include "spiffs.h"

static const char *TAG = "MP3_PLAYER";

#define MP3_SQUAWK_FILE_PATH "/spiffs/squawk.mp3"

#define AUDIO_SD_PIN GPIO_NUM_33

bool play_audio = false;
float volume = 0.5f; // Volume control (0.0 to 1.0)

esp_err_t mute_audio(bool mute)
{
    ESP_LOGI(TAG, "mute setting %d", mute);
    gpio_set_level(AUDIO_SD_PIN, mute ? 0 : 1);
    return ESP_OK;
}

void configure_pins() {
    // Configure SD pin for PAM8302A
    gpio_reset_pin(AUDIO_SD_PIN);
    gpio_set_direction(AUDIO_SD_PIN, GPIO_MODE_OUTPUT);
}

void audio_player_task(void *param) {
    ESP_LOGI(TAG, "Initializing audio player...");

    // Initialize the DAC
    dac_channel_t channel = DAC_CHANNEL_1;  // DAC1 is GPIO 25
    dac_output_enable(channel);

    // Configure pins
    configure_pins();
    mute_audio(true);  // Mute 

    HMP3Decoder hMP3Decoder;
    MP3FrameInfo mp3FrameInfo;
    hMP3Decoder = MP3InitDecoder();
    if (hMP3Decoder == NULL) {
        ESP_LOGE(TAG, "Failed to initialize MP3 decoder");
        vTaskDelete(NULL);
        return;
    } else {
        ESP_LOGI(TAG, "MP3 decoder initialized successfully");
    }

    size_t mp3_size;
    uint8_t *mp3_data = read_mp3_file(MP3_SQUAWK_FILE_PATH, &mp3_size);
    if (mp3_data == NULL) {
        ESP_LOGE(TAG, "Failed to read MP3 file");
        MP3FreeDecoder(hMP3Decoder);
        vTaskDelete(NULL);
        return;
    }

    uint8_t *readPtr = mp3_data;
    uint8_t outputBuffer[1152 * 2 * sizeof(short)];
    int bytesLeft = mp3_size;
    int offset;

    while (true) {
        ESP_LOGI(TAG, "Waiting for semaphore");
        if (xSemaphoreTake(audioSemaphore, portMAX_DELAY) == pdTRUE) {
            ESP_LOGI(TAG, "Semaphore taken. Checking audio playback status");
            mute_audio(false);  // Unmute (set SD pin high)
            if (play_audio) {
                ESP_LOGI(TAG, "Starting MP3 playback. MP3 size: %d", mp3_size);
                readPtr = mp3_data;
                bytesLeft = mp3_size;

                while (bytesLeft > 0) {
                    offset = MP3FindSyncWord(readPtr, bytesLeft);
                    if (offset < 0) {
                        ESP_LOGE(TAG, "MP3 sync word not found");
                        break;
                    }
                    readPtr += offset;
                    bytesLeft -= offset;

                    int err = MP3Decode(hMP3Decoder, &readPtr, &bytesLeft, (short *)outputBuffer, 0);
                    if (err != ERR_MP3_NONE) {
                        ESP_LOGE(TAG, "MP3 decode error: %d", err);
                        break;
                    }

                    MP3GetLastFrameInfo(hMP3Decoder, &mp3FrameInfo);

                    // Write PCM data to DAC with volume control
                    for (int i = 0; i < mp3FrameInfo.outputSamps; i++) {
                        int16_t sample = ((short *)outputBuffer)[i];
                        sample = (int16_t)(sample * volume);  // Apply volume control
                        uint8_t dac_value = (sample + 32768) >> 8;  // Convert 16-bit PCM to 8-bit DAC value
                        dac_output_voltage(channel, dac_value);
                        vTaskDelay(pdMS_TO_TICKS(1));  // Adjust delay as needed
                    }
                }
                mute_audio(true);  // Mute (set SD pin low)
            }
        } else {
            ESP_LOGE(TAG, "Failed to take semaphore");
        }
    }

    free(mp3_data);
    MP3FreeDecoder(hMP3Decoder);
    vTaskDelete(NULL);
}

void set_audio_playback(bool status)
{
    play_audio = status;
    if (play_audio) {
        if (audioSemaphore != NULL) {
            ESP_LOGI(TAG, "Giving semaphore");
            xSemaphoreGive(audioSemaphore);
        } else {
            ESP_LOGE(TAG, "audioSemaphore is NULL");
        }
    }
}

void set_volume(float new_volume) {
    if (new_volume < 0.0f) new_volume = 0.0f;
    if (new_volume > 1.0f) new_volume = 1.0f;
    volume = new_volume;
    ESP_LOGI(TAG, "Volume set to %.2f", volume);
}
