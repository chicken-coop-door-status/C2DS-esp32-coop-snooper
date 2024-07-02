#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_log.h"
#include "audio_player.h"
#include "driver/gpio.h"
#include "mp3.h"

static const char *TAG = "MP3_PLAYER";

extern const uint8_t squawk_mp3_start[] asm("_binary_squawk_mp3_start");
extern const uint8_t squawk_mp3_end[] asm("_binary_squawk_mp3_end");

i2s_chan_handle_t i2s_tx_chan;
i2s_chan_handle_t i2s_rx_chan;

esp_err_t bsp_i2s_write(void * audio_buffer, size_t len, size_t *bytes_written, uint32_t timeout_ms)
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

esp_err_t audio_mute_function(AUDIO_PLAYER_MUTE_SETTING setting) {
    ESP_LOGI(TAG, "mute setting %d", setting);
    return ESP_OK;
}

void audio_player_task(void *param) {
    audio_player_config_t config = {
        .mute_fn = audio_mute_function,
        .write_fn = bsp_i2s_write,
        .clk_set_fn = bsp_i2s_reconfig_clk,
        .priority = 0,
        .coreID = 0
    };
    
    ESP_ERROR_CHECK(audio_player_new(config));

    const uint8_t *mp3_start = squawk_mp3_start;
    const uint8_t *mp3_end = squawk_mp3_end;
    size_t mp3_size = mp3_end - mp3_start;

    FILE *fp = fmemopen((void*)mp3_start, mp3_size, "rb");
    if (fp == NULL) {
        ESP_LOGE(TAG, "Failed to open MP3 file");
        vTaskDelete(NULL);
        return;
    }

    ESP_ERROR_CHECK(audio_player_play(fp));

    while (audio_player_get_state() == AUDIO_PLAYER_STATE_PLAYING) {
        vTaskDelay(pdMS_TO_TICKS(100));
    }

    fclose(fp);
    ESP_ERROR_CHECK(audio_player_delete());

    vTaskDelete(NULL);
}

esp_err_t bsp_audio_init(const i2s_std_config_t *i2s_config, i2s_chan_handle_t *tx_channel, i2s_chan_handle_t *rx_channel)
{
    /* Setup I2S peripheral */
    i2s_chan_config_t chan_cfg = I2S_CHANNEL_DEFAULT_CONFIG(CONFIG_BSP_I2S_NUM, I2S_ROLE_MASTER);
    chan_cfg.auto_clear = true; // Auto clear the legacy data in the DMA buffer
    ESP_ERROR_CHECK(i2s_new_channel(&chan_cfg, tx_channel, rx_channel));

    /* Setup I2S channels */
    const i2s_std_config_t std_cfg_default = BSP_I2S_DUPLEX_MONO_CFG(22050);
    const i2s_std_config_t *p_i2s_cfg = &std_cfg_default;
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

    /* Setup power amplifier pin */
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
