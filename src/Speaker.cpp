#include "Speaker.h"
#include "audio_globals.h"
#include "logger.h"

bool Speaker::initialize()
{
    gLogger.info("Speaker init ...");

    // Drive SD_MODE low before the I2S clocks exist. Most breakouts pull
    // SD_MODE up to VIN, so a floating pin at reset enables the amp and
    // produces a startup pop. The datasheet also requires SD_MODE at 0 V
    // whenever BCLK and WS are stopped, which is the state right now.
    gpio_config_t sd_conf = {};
    sd_conf.pin_bit_mask = (1ULL << PIN_AMP_SD_MODE);
    sd_conf.mode = GPIO_MODE_OUTPUT;
    sd_conf.pull_up_en = GPIO_PULLUP_DISABLE;
    sd_conf.pull_down_en = GPIO_PULLDOWN_DISABLE;
    sd_conf.intr_type = GPIO_INTR_DISABLE;

    esp_err_t err = gpio_config(&sd_conf);

    if (err != ESP_OK)
    {
        gLogger.error("Speaker SD_MODE gpio_config FAILED err:0x%x", (unsigned)err);
        return false;
    }

    gpio_set_level((gpio_num_t)PIN_AMP_SD_MODE, AMP_SD_MODE_SHUTDOWN);

    i2s_chan_config_t chan_conf = I2S_CHANNEL_DEFAULT_CONFIG(I2S_NUM_0, I2S_ROLE_MASTER);
    chan_conf.auto_clear = true;

    err = i2s_new_channel(&chan_conf, &m_txHandle, nullptr);

    if (err != ESP_OK)
    {
        gLogger.error("Speaker i2s_new_channel FAILED err:0x%x", (unsigned)err);
        return false;
    }

    i2s_std_config_t std_conf = {};
    std_conf.clk_cfg = I2S_STD_CLK_DEFAULT_CONFIG(AUDIO_SAMPLE_RATE);
    std_conf.slot_cfg = I2S_STD_PHILIPS_SLOT_DEFAULT_CONFIG(I2S_DATA_BIT_WIDTH_32BIT, I2S_SLOT_MODE_STEREO);
    std_conf.gpio_cfg.mclk = I2S_GPIO_UNUSED;
    std_conf.gpio_cfg.bclk = (gpio_num_t)PIN_I2S_BCLK;
    std_conf.gpio_cfg.ws = (gpio_num_t)PIN_I2S_WS;
    std_conf.gpio_cfg.dout = (gpio_num_t)PIN_I2S_AMP_DIN;
    std_conf.gpio_cfg.din = I2S_GPIO_UNUSED;
    std_conf.gpio_cfg.invert_flags.mclk_inv = false;
    std_conf.gpio_cfg.invert_flags.bclk_inv = false;
    std_conf.gpio_cfg.invert_flags.ws_inv = false;

    err = i2s_channel_init_std_mode(m_txHandle, &std_conf);

    if (err != ESP_OK)
    {
        gLogger.error("Speaker i2s_channel_init_std_mode FAILED err:0x%x", (unsigned)err);
        return false;
    }

    err = i2s_channel_enable(m_txHandle);

    if (err != ESP_OK)
    {
        gLogger.error("Speaker i2s_channel_enable FAILED err:0x%x", (unsigned)err);
        return false;
    }

    m_ready = true;
    gLogger.info("Speaker init ok");
    return true;
}

bool Speaker::enable()
{
    if (m_ready == false)
    {
        return false;
    }

    // Clocks are already running from i2s_channel_enable, so it is safe to
    // raise SD_MODE now. Doing it in this order avoids the click the datasheet
    // warns about when SD_MODE leads the clocks.
    gpio_set_level((gpio_num_t)PIN_AMP_SD_MODE, AMP_SD_MODE_ENABLE);
    gLogger.info("Speaker amp enabled");
    return true;
}

bool Speaker::shutdown()
{
    // SD_MODE must return to 0 V before the clocks stop. This test never stops
    // the clocks, but the mute path uses this same call.
    gpio_set_level((gpio_num_t)PIN_AMP_SD_MODE, AMP_SD_MODE_SHUTDOWN);
    gLogger.info("Speaker amp shutdown");
    return true;
}

bool Speaker::writeSamples(const int16_t *p_samples, size_t sample_count)
{
    if (m_ready == false)
    {
        return false;
    }

    if (p_samples == nullptr)
    {
        gLogger.error("Speaker writeSamples null buffer FAILED");
        return false;
    }

    // Each frame is one stereo pair of 32-bit slots. The int16 sample sits in
    // the top 16 bits of the left slot; the right slot is silent because the
    // amp only reads the left channel. Index into the buffer explicitly rather
    // than walking pointers.
    static int32_t s_slotBuf[256];
    size_t written_total = 0;

    while (written_total < sample_count)
    {
        size_t chunk = sample_count - written_total;

        if (chunk > 128)
        {
            chunk = 128;
        }

        for (size_t i = 0; i < chunk; i++)
        {
            int32_t s = (int32_t)p_samples[written_total + i] << 16;
            s_slotBuf[i * 2 + 0] = s;
            s_slotBuf[i * 2 + 1] = 0;
        }

        size_t bytes_to_write = chunk * 2 * sizeof(int32_t);
        size_t bytes_written = 0;

        esp_err_t err = i2s_channel_write(m_txHandle, s_slotBuf, bytes_to_write, &bytes_written, 100);

        if (err != ESP_OK)
        {
            gLogger.error("Speaker i2s_channel_write FAILED err:0x%x", (unsigned)err);
            return false;
        }

        written_total += chunk;
    }

    return true;
}
