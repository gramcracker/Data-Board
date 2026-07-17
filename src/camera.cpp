#include "camera.h"
#include "camera_globals.h"
#include "pins.h"
#include "logger.h"

#include <cstring>
#include <cstdlib>

#include "my_esp32_camera.h"
#include "img_converters.h"

extern Logger gLogger;

bool Camera::initialize()
{
    camera_config_t config;

    memset(&config, 0, sizeof(config));

    config.pin_pwdn      = -1;
    config.pin_reset     = -1;
    config.pin_xclk      = PIN_CAM_XCLK;
    config.pin_sccb_sda  = PIN_CAM_SDA;
    config.pin_sccb_scl  = PIN_CAM_SCL;
    config.pin_d0        = PIN_CAM_D0;
    config.pin_d1        = PIN_CAM_D1;
    config.pin_d2        = PIN_CAM_D2;
    config.pin_d3        = PIN_CAM_D3;
    // 4-bit mode: D4-D7 are unwired. PIN_CAM_D4..D7 are -1 (see pins.h) so the
    // driver does not claim the audio pins on GPIO 15/16/40/41. If this fork
    // rejects -1 data lines and the camera regresses, that is the first thing
    // to revert.
    config.pin_d4        = PIN_CAM_D4;
    config.pin_d5        = PIN_CAM_D5;
    config.pin_d6        = PIN_CAM_D6;
    config.pin_d7        = PIN_CAM_D7;
    config.pin_vsync     = PIN_CAM_VSYNC;
    config.pin_href      = PIN_CAM_HREF;
    config.pin_pclk      = PIN_CAM_PCLK;
    config.xclk_freq_hz  = CAM_XCLK_FREQ_HZ;
    config.ledc_timer    = LEDC_TIMER_0;
    config.ledc_channel  = LEDC_CHANNEL_0;
    config.sccb_i2c_port = -1;
    config.pixel_format  = PIXFORMAT_GRAYSCALE;
    config.frame_size    = FRAMESIZE_QVGA;
    config.fb_count      = CAM_FB_COUNT;
    config.fb_location   = CAMERA_FB_IN_PSRAM;
    config.grab_mode     = CAMERA_GRAB_LATEST;

    gLogger.info("Camera init ...");

    esp_err_t err = my_esp_camera_init(&config);

    if (err != ESP_OK)
    {
        gLogger.error("Camera init FAILED err:0x%x", (unsigned)err);
        m_ready = false;
        return false;
    }

    m_ready = false;

    sensor_t *p_sensor = my_esp_camera_sensor_get();

    if (p_sensor == nullptr)
    {
        gLogger.error("Camera sensor handle NULL");
        return false;
    }

    // The breakout only wires D0-D3, so force 4-bit serial output regardless of
    // what the sensor's default table set. In 4-bit mode each 8-bit pixel is
    // sent as two low-nibble bytes, which captureFrame reassembles.
    p_sensor->set_reg(p_sensor, 0x3059, 0xFF, 0x42);
    p_sensor->set_reg(p_sensor, 0x0104, 0xFF, 0x01);

    if (CAM_START_WITH_TEST_PATTERN == 1)
    {
        p_sensor->set_colorbar(p_sensor, 1);
    }

    m_ready = true;
    gLogger.info("Camera init ok");

    return true;
}

bool Camera::captureFrame(uint8_t *p_buffer, size_t buffer_len, size_t &out_len)
{
    if (m_ready == false)
    {
        return false;
    }

    if (buffer_len < CAM_FRAME_BYTES)
    {
        gLogger.error("Camera dest buffer too small buf:%u need:%u", (unsigned)buffer_len, (unsigned)CAM_FRAME_BYTES);
        return false;
    }

    camera_fb_t *p_fb = my_esp_camera_fb_get();

    if (p_fb == nullptr)
    {
        gLogger.error("Camera fb_get FAILED");
        return false;
    }

    size_t need = (size_t)CAM_RAW_LINE_BYTES * (size_t)CAM_RAW_LINES;

    if (p_fb->len < need)
    {
        gLogger.error("Camera frame short len:%u need:%u", (unsigned)p_fb->len, (unsigned)need);
        my_esp_camera_fb_return(p_fb);
        return false;
    }

    // Each pixel arrives as two consecutive bytes carrying its low and high
    // nibbles on D0-D3. Mask off the unwired upper lines, recombine, and crop
    // the dummy border down to CAM_WIDTH x CAM_HEIGHT.
    for (int y = 0; y < CAM_HEIGHT; y++)
    {
        const uint8_t *p_raw = p_fb->buf + ((size_t)(y + CAM_CROP_Y) * (size_t)CAM_RAW_LINE_BYTES);
        uint8_t       *p_dst = p_buffer + ((size_t)y * (size_t)CAM_WIDTH);

        for (int x = 0; x < CAM_WIDTH; x++)
        {
            int idx = (CAM_CROP_X + x) * 2;
            int first = p_raw[idx] & 0x0F;
            int second = p_raw[idx + 1] & 0x0F;

            if (CAM_NIBBLE_LSB_FIRST == 1)
            {
                p_dst[x] = (uint8_t)((second << 4) | first);
            }
            else
            {
                p_dst[x] = (uint8_t)((first << 4) | second);
            }
        }
    }

    out_len = (size_t)(CAM_WIDTH * CAM_HEIGHT);

    m_lastLen = p_fb->len;
    m_frameCount = m_frameCount + 1;

    my_esp_camera_fb_return(p_fb);

    return true;
}

bool Camera::encodeJpeg(const uint8_t *p_gray, size_t gray_len, uint8_t **pp_out, size_t &out_len)
{
    bool ok = fmt2jpg((uint8_t *)p_gray, gray_len, CAM_WIDTH, CAM_HEIGHT, PIXFORMAT_GRAYSCALE, CAM_JPEG_QUALITY, pp_out, &out_len);

    if (ok == false)
    {
        gLogger.error("Camera JPEG encode FAILED");
        return false;
    }

    return true;
}

bool Camera::freeJpeg(uint8_t *p_jpeg)
{
    if (p_jpeg != nullptr)
    {
        free(p_jpeg);
    }

    return true;
}

bool Camera::getCaptureStats(uint32_t &out_frames, size_t &out_last_len)
{
    out_frames = m_frameCount;
    out_last_len = m_lastLen;

    return true;
}