#include "mp3.h"

#include "driver/gpio.h"
#include "driver/i2s.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/semphr.h"
#include "freertos/task.h"
#include "mp3dec.h"
#include "sdkconfig.h"
#include "squawk_mp3.h" // Include the generated header file
#include "string.h"

static const char *TAG = "MP3_PLAYER";

#define I2S_NUM I2S_NUM_0
#define I2S_WS_PIN GPIO_NUM_5  // LRC
#define I2S_BCK_PIN GPIO_NUM_6 // BCLK
#define I2S_DO_PIN GPIO_NUM_7  // DIN
#define GAIN_PIN GPIO_NUM_9    // GAIN
#define I2S_SD_PIN GPIO_NUM_10 // SD
#define SAMPLE_RATE 44100      // Audio sample rate

bool play_audio = false;
float volume = 1.0f;         // Volume control (0.0 to 1.0)
i2s_chan_handle_t tx_handle; // Moved tx_handle to global scope

void configure_i2s()
{
    i2s_config_t i2s_config = {.mode = I2S_MODE_MASTER | I2S_MODE_TX,
                               .sample_rate = SAMPLE_RATE,
                               .bits_per_sample = I2S_BITS_PER_SAMPLE_16BIT,
                               .channel_format = I2S_CHANNEL_FMT_RIGHT_LEFT,
                               .communication_format = I2S_COMM_FORMAT_I2S,
                               .intr_alloc_flags = ESP_INTR_FLAG_LEVEL1,
                               .dma_buf_count = 8,
                               .dma_buf_len = 64,
                               .use_apll = false,
                               .tx_desc_auto_clear = true,
                               .fixed_mclk = 0};

    i2s_pin_config_t pin_config = {.bck_io_num = I2S_BCK_PIN,
                                   .ws_io_num = I2S_WS_PIN,
                                   .data_out_num = I2S_DO_PIN,
                                   .data_in_num = I2S_PIN_NO_CHANGE};

    // Configure and install I2S driver
    ESP_ERROR_CHECK(i2s_driver_install(I2S_NUM, &i2s_config, 0, NULL));
    ESP_ERROR_CHECK(i2s_set_pin(I2S_NUM, &pin_config));

    // Configure SD pin for controlling the amplifier
    gpio_reset_pin(I2S_SD_PIN);
    gpio_set_direction(I2S_SD_PIN, GPIO_MODE_OUTPUT);
    gpio_set_level(I2S_SD_PIN, 1); // Enable the amplifier by default

    // Configure GAIN pin for controlling the gain
    gpio_reset_pin(GAIN_PIN);
    gpio_set_direction(GAIN_PIN, GPIO_MODE_OUTPUT);
    gpio_set_level(GAIN_PIN, 0); // Set default gain to low (3dB)
}

void set_gain(bool high_gain)
{
    gpio_set_level(GAIN_PIN, high_gain ? 1 : 0);
    ESP_LOGI(TAG, "Gain set to %s", high_gain ? "9dB" : "3dB");
}

void enable_amplifier(bool enable)
{
    gpio_set_level(I2S_SD_PIN, enable ? 1 : 0);
    ESP_LOGI(TAG, "Amplifier %s", enable ? "enabled" : "disabled");
}

void audio_player_task(void *param)
{
    ESP_LOGI(TAG, "Initializing audio player...");

    // Configure I2S
    configure_i2s();

    HMP3Decoder hMP3Decoder;
    MP3FrameInfo mp3FrameInfo;
    hMP3Decoder = MP3InitDecoder();
    if (hMP3Decoder == NULL)
    {
        ESP_LOGE(TAG, "Failed to initialize MP3 decoder");
        vTaskDelete(NULL);
        return;
    }
    else
    {
        ESP_LOGI(TAG, "MP3 decoder initialized successfully");
    }

    uint8_t *mp3_data = squawk_mp3;
    size_t mp3_size = squawk_mp3_len;
    uint8_t *readPtr = mp3_data;
    uint8_t outputBuffer[1152 * 2 * sizeof(short)];
    int bytesLeft = mp3_size;
    int offset;

    while (true)
    {
        ESP_LOGI(TAG, "Waiting for semaphore");
        if (xSemaphoreTake(audioSemaphore, portMAX_DELAY) == pdTRUE)
        {
            ESP_LOGI(TAG, "Semaphore taken. Checking audio playback status");
            if (play_audio)
            {
                for (int play_count = 0; play_count < 3; play_count++)
                {
                    ESP_LOGI(TAG, "Starting MP3 playback #%d. MP3 size: %d", play_count + 1, mp3_size);
                    readPtr = mp3_data;
                    bytesLeft = mp3_size;

                    while (bytesLeft > 0)
                    {
                        offset = MP3FindSyncWord(readPtr, bytesLeft);
                        if (offset < 0)
                        {
                            ESP_LOGE(TAG, "MP3 sync word not found");
                            break;
                        }
                        readPtr += offset;
                        bytesLeft -= offset;

                        int err = MP3Decode(hMP3Decoder, &readPtr, &bytesLeft, (short *)outputBuffer, 0);
                        if (err != ERR_MP3_NONE)
                        {
                            ESP_LOGE(TAG, "MP3 decode error: %d", err);
                            break;
                        }

                        MP3GetLastFrameInfo(hMP3Decoder, &mp3FrameInfo);

                        // Write PCM data to I2S with volume control
                        size_t bytes_written = 0;
                        for (int i = 0; i < mp3FrameInfo.outputSamps; i++)
                        {
                            int16_t sample = ((short *)outputBuffer)[i];
                            sample = (int16_t)(sample * volume); // Apply volume control
                            i2s_write(I2S_NUM, &sample, sizeof(sample), &bytes_written, portMAX_DELAY);
                        }
                    }
                }
            }
        }
        else
        {
            ESP_LOGE(TAG, "Failed to take semaphore");
        }
    }

    MP3FreeDecoder(hMP3Decoder);
    vTaskDelete(NULL);
}

void set_audio_playback(bool status)
{
    play_audio = status;
    if (play_audio)
    {
        if (audioSemaphore != NULL)
        {
            ESP_LOGI(TAG, "Giving semaphore");
            xSemaphoreGive(audioSemaphore);
        }
        else
        {
            ESP_LOGE(TAG, "audioSemaphore is NULL");
        }
    }
}

void set_volume(float new_volume)
{
    if (new_volume < 0.0f)
        new_volume = 0.0f;
    if (new_volume > 1.0f)
        new_volume = 1.0f;
    volume = new_volume;
    ESP_LOGI(TAG, "Volume set to %.2f", volume);
}
