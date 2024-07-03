#include "string.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/semphr.h"
#include "esp_log.h"
#include "driver/gpio.h"
#include "driver/dac.h"
#include "driver/gptimer.h"
#include "mp3.h"
#include "mp3dec.h"
#include "spiffs.h"
#include "squawk_mp3.h"  // Include the generated header file

static const char *TAG = "MP3_PLAYER";

#define AUDIO_SD_PIN GPIO_NUM_33
#define SAMPLE_RATE 44100  // Audio sample rate

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

bool IRAM_ATTR timer_callback(gptimer_handle_t timer, const gptimer_alarm_event_data_t *edata, void *user_ctx) {
    BaseType_t high_task_awoken = pdFALSE;
    xSemaphoreGiveFromISR(timer_semaphore, &high_task_awoken);
    return high_task_awoken == pdTRUE;
}

void audio_player_task(void *param) {
    ESP_LOGI(TAG, "Initializing audio player...");

    // Initialize the DAC
    dac_output_enable(DAC_CHAN_0);  // DAC1 is GPIO 25

    // Configure pins
    configure_pins();
    mute_audio(true);  // Mute 

    // Initialize GPTimer
    gptimer_handle_t gptimer = NULL;
    gptimer_config_t timer_config = {
        .clk_src = GPTIMER_CLK_SRC_DEFAULT,
        .direction = GPTIMER_COUNT_UP,
        .resolution_hz = SAMPLE_RATE * 2  // Resolution in Hz
    };
    ESP_ERROR_CHECK(gptimer_new_timer(&timer_config, &gptimer));

    gptimer_event_callbacks_t cbs = {
        .on_alarm = timer_callback
    };
    ESP_ERROR_CHECK(gptimer_register_event_callbacks(gptimer, &cbs, NULL));

    gptimer_alarm_config_t alarm_config = {
        .alarm_count = 1,
        .reload_count = 0,
        .flags = {
            .auto_reload_on_alarm = true
        }
    };
    ESP_ERROR_CHECK(gptimer_set_alarm_action(gptimer, &alarm_config));

    ESP_ERROR_CHECK(gptimer_enable(gptimer));

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

    uint8_t *mp3_data = squawk_mp3;
    size_t mp3_size = squawk_mp3_len;
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

                ESP_ERROR_CHECK(gptimer_start(gptimer));

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
                    for (int i = 0; i < mp3FrameInfo.outputSamps; i += 2) {
                        int16_t left_sample = ((short *)outputBuffer)[i];
                        int16_t right_sample = ((short *)outputBuffer)[i + 1];

                        // Downmix to mono by averaging left and right samples
                        int16_t mono_sample = (left_sample + right_sample) / 2;

                        mono_sample = (int16_t)(mono_sample * volume);  // Apply volume control

                        uint8_t dac_value = (mono_sample + 32768) >> 8;  // Convert 16-bit PCM to 8-bit DAC value

                        if (xSemaphoreTake(timer_semaphore, portMAX_DELAY) == pdTRUE) {
                            dac_output_voltage(DAC_CHAN_0, dac_value);
                        }
                    }
                }
                ESP_ERROR_CHECK(gptimer_stop(gptimer));
                mute_audio(true);  // Mute (set SD pin low)
            }
        } else {
            ESP_LOGE(TAG, "Failed to take semaphore");
        }
    }

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