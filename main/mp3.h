#ifndef SNOOPER_MP3_H
#define SNOOPER_MP3_H

#include "driver/i2s_std.h"
#include "esp_err.h"
#include "freertos/FreeRTOS.h"
#include "freertos/semphr.h"
#include "mp3dec.h"

#define CONFIG_BSP_I2S_NUM 1

#ifndef I2S_PIN_NO_CHANGE
#define I2S_PIN_NO_CHANGE (-1)
#endif

extern i2s_chan_handle_t i2s_tx_chan;
extern i2s_chan_handle_t i2s_rx_chan;

extern SemaphoreHandle_t audioSemaphore;
extern SemaphoreHandle_t timer_semaphore;

/* Audio */
#define BSP_I2S_SCLK (GPIO_NUM_14)
#define BSP_I2S_MCLK (GPIO_NUM_2)  // If needed, else set to -1 or I2S_PIN_NO_CHANGE
#define BSP_I2S_LCLK (GPIO_NUM_27)
#define BSP_I2S_DOUT (GPIO_NUM_21)        // To Codec ES8311
#define BSP_I2S_DSIN (I2S_PIN_NO_CHANGE)  // Not used in this configuration
#define BSP_POWER_AMP_IO (GPIO_NUM_46)
#define BSP_MUTE_STATUS (GPIO_NUM_1)

#define BSP_I2S_GPIO_CFG           \
    {                              \
        .mclk = BSP_I2S_MCLK,      \
        .bclk = BSP_I2S_SCLK,      \
        .ws = BSP_I2S_LCLK,        \
        .dout = BSP_I2S_DOUT,      \
        .din = BSP_I2S_DSIN,       \
        .invert_flags =            \
            {                      \
                .mclk_inv = false, \
                .bclk_inv = false, \
                .ws_inv = false,   \
            },                     \
    }

#define BSP_I2S_DUPLEX_MONO_CFG(_sample_rate)                                                         \
    {                                                                                                 \
        .clk_cfg = I2S_STD_CLK_DEFAULT_CONFIG(_sample_rate),                                          \
        .slot_cfg = I2S_STD_PHILIP_SLOT_DEFAULT_CONFIG(I2S_DATA_BIT_WIDTH_16BIT, I2S_SLOT_MODE_MONO), \
        .gpio_cfg = BSP_I2S_GPIO_CFG,                                                                 \
    }

// Function to initialize I2S peripheral
esp_err_t bsp_audio_init(const i2s_std_config_t *i2s_config, i2s_chan_handle_t *tx_channel,
                         i2s_chan_handle_t *rx_channel);

// Function to mute/unmute audio
esp_err_t audio_mute_function(int setting);

// Function to reconfigure I2S clock
esp_err_t bsp_i2s_reconfig_clk(uint32_t rate, uint32_t bits_cfg, i2s_slot_mode_t ch);

// Function to write audio data to I2S
esp_err_t bsp_i2s_write(void *audio_buffer, size_t len, size_t *bytes_written, uint32_t timeout_ms);

// FreeRTOS task for MP3 playback
void audio_player_task(void *param);

// Function to set the audio playback status
void set_audio_playback(bool status);

void set_gain(bool high_gain);

void enable_amplifier(bool enable);

void set_volume(float new_volume);

#endif  // SNOOPER_MP3_H
