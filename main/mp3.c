#include "string.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/semphr.h"
#include "esp_log.h"
#include "driver/gpio.h"
#include "driver/i2s_std.h"
#include "mp3.h"
#include "mp3dec.h"
#include "spiffs.h"

static const char *TAG = "MP3_PLAYER";

#define MP3_SQUAWK_FILE_PATH "/spiffs/squawk.mp3"
#define MP3_CLUCKING_FILE_PATH "/spiffs/clucking.mp3"

i2s_chan_handle_t i2s_tx_chan;
i2s_chan_handle_t i2s_rx_chan;

bool play_audio = false;

esp_err_t bsp_i2s_write(void *audio_buffer, size_t len, size_t *bytes_written, uint32_t timeout_ms)
{
    return i2s_channel_write(i2s_tx_chan, (char *)audio_buffer, len, bytes_written, timeout_ms);
}


esp_err_t bsp_i2s_reconfig_clk(uint32_t rate, uint32_t bits_cfg, i2s_slot_mode_t ch)
{
    esp_err_t ret = ESP_OK;
    i2s_std_config_t std_cfg = {
        .clk_cfg = I2S_STD_CLK_DEFAULT_CONFIG(rate),
        .slot_cfg = I2S_STD_PHILIP_SLOT_DEFAULT_CONFIG((i2s_data_bit_width_t)bits_cfg, (i2s_slot_mode_t)ch),
        .gpio_cfg = BSP_I2S_GPIO_CFG,
    };

    ret |= i2s_channel_disable(i2s_tx_chan);
    ret |= i2s_channel_reconfig_std_clock(i2s_tx_chan, &std_cfg.clk_cfg);
    ret |= i2s_channel_reconfig_std_slot(i2s_tx_chan, &std_cfg.slot_cfg);
    ret |= i2s_channel_enable(i2s_tx_chan);
    return ret;
}

esp_err_t audio_mute_function(int setting)
{
    ESP_LOGI(TAG, "mute setting %d", setting);
    return ESP_OK;
}

void audio_player_task(void *param) {
    ESP_LOGI(TAG, "Initializing audio player...");

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
                        switch (err) {
                            case ERR_MP3_INDATA_UNDERFLOW:
                                ESP_LOGE(TAG, "ERR_MP3_INDATA_UNDERFLOW");
                                break;
                            case ERR_MP3_MAINDATA_UNDERFLOW:
                                ESP_LOGE(TAG, "ERR_MP3_MAINDATA_UNDERFLOW");
                                break;
                            case ERR_MP3_FREE_BITRATE_SYNC:
                                ESP_LOGE(TAG, "ERR_MP3_FREE_BITRATE_SYNC");
                                break;
                            default:
                                ESP_LOGE(TAG, "Unknown MP3 decode error");
                                break;
                        }
                        break;
                    }

                    MP3GetLastFrameInfo(hMP3Decoder, &mp3FrameInfo);
                    ESP_LOGI(TAG, "Decoded MP3 frame. Bitrate: %d, Channels: %d, SampleRate: %d",
                             mp3FrameInfo.bitrate, mp3FrameInfo.nChans, mp3FrameInfo.samprate);

                    size_t bytes_written;
                    esp_err_t ret = bsp_i2s_write(outputBuffer, mp3FrameInfo.outputSamps * sizeof(short), &bytes_written, portMAX_DELAY);
                    if (ret != ESP_OK) {
                        ESP_LOGE(TAG, "Failed to write to I2S: %s", esp_err_to_name(ret));
                        break;
                    }

                    vTaskDelay(pdMS_TO_TICKS(20));
                }
            }
        } else {
            ESP_LOGE(TAG, "Failed to take semaphore");
        }
    }

    free(mp3_data);
    MP3FreeDecoder(hMP3Decoder);
    vTaskDelete(NULL);
}


esp_err_t bsp_audio_init(const i2s_std_config_t *i2s_config, i2s_chan_handle_t *tx_channel, i2s_chan_handle_t *rx_channel)
{
    // I2S configuration specific to your setup
    i2s_std_config_t i2s_config_local = {
        .clk_cfg = I2S_STD_CLK_DEFAULT_CONFIG(44100),
        .slot_cfg = I2S_STD_PHILIP_SLOT_DEFAULT_CONFIG(I2S_DATA_BIT_WIDTH_16BIT, I2S_SLOT_MODE_STEREO),
        .gpio_cfg = {
            .bclk = GPIO_NUM_26,      // Bit clock
            .ws = GPIO_NUM_45,       // Word select (LRCK)
            .dout = GPIO_NUM_21,     // Data out
            .din = I2S_GPIO_UNUSED,  // Not used
        },
    };

    i2s_chan_config_t chan_cfg = I2S_CHANNEL_DEFAULT_CONFIG(CONFIG_BSP_I2S_NUM, I2S_ROLE_MASTER);
    chan_cfg.auto_clear = true; 
    ESP_ERROR_CHECK(i2s_new_channel(&chan_cfg, tx_channel, rx_channel));

    const i2s_std_config_t *p_i2s_cfg = &i2s_config_local;
    if (i2s_config != NULL) {
        p_i2s_cfg = i2s_config;
    }

    if (tx_channel != NULL) {
        ESP_ERROR_CHECK(i2s_channel_init_std_mode(*tx_channel, p_i2s_cfg));
        ESP_ERROR_CHECK(i2s_channel_enable(*tx_channel));
    }
    if (rx_channel != NULL) {
        ESP_ERROR_CHECK(i2s_channel_init_std_mode(*rx_channel, p_i2s_cfg));
        ESP_ERROR_CHECK(i2s_channel_enable(*rx_channel));
    }

    const gpio_config_t io_conf = {
        .intr_type = GPIO_INTR_DISABLE,
        .mode = GPIO_MODE_OUTPUT,
        .pin_bit_mask = BIT64(BSP_POWER_AMP_IO),
        .pull_down_en = GPIO_PULLDOWN_DISABLE,
        .pull_up_en = GPIO_PULLDOWN_DISABLE,
    };
    ESP_ERROR_CHECK(gpio_config(&io_conf));

    return ESP_OK;
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
