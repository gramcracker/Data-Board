#include "camera_globals.h"
#include "camera.h"
#include "pins.h"
#include "logger.h"
#include <Arduino.h>
#include <Wire.h>
#include <cstdlib>
#include "esp_camera.h"
#include "img_converters.h"
#include "esp_cam_ctlr.h"
#include "esp_cam_ctlr_dvp.h"
#include "hal/cam_ctlr_types.h"
#include "esp_heap_caps.h"

namespace
{
    esp_cam_ctlr_handle_t s_camHandle = nullptr;
    uint8_t              *s_pRaw[2] = { nullptr, nullptr };
    size_t                s_rawLen = 0;
    volatile int          s_fillIdx = 1;
    volatile int          s_readyIdx = -1;
    volatile bool         s_frameReady = false;
    volatile size_t       s_latestLen = 0;
    volatile uint32_t     s_frameCount = 0;

    bool IRAM_ATTR camOnGetNewTrans(esp_cam_ctlr_handle_t handle, esp_cam_ctlr_trans_t *trans, void *user_data)
    {
        (void)handle;
        (void)user_data;

        if (s_fillIdx == 0)
        {
            s_fillIdx = 1;
        }
        else
        {
            s_fillIdx = 0;
        }

        trans->buffer = s_pRaw[s_fillIdx];
        trans->buflen = s_rawLen;

        return false;
    }

    bool IRAM_ATTR camOnTransFinished(esp_cam_ctlr_handle_t handle, esp_cam_ctlr_trans_t *trans, void *user_data)
    {
        (void)handle;
        (void)user_data;

        int done_idx = 0;

        if (trans->buffer == s_pRaw[1])
        {
            done_idx = 1;
        }

        s_readyIdx = done_idx;
        s_latestLen = trans->received_size;
        s_frameReady = true;
        s_frameCount = s_frameCount + 1;

        return false;
    }
}

static const CameraReg HM01B0_INIT_TABLE[] =
{
    { 0x1003, 0x08 },
    { 0x1007, 0x08 },
    { 0x3044, 0x0A },
    { 0x3045, 0x00 },
    { 0x3047, 0x0A },
    { 0x3050, 0xC0 },
    { 0x3051, 0x42 },
    { 0x3052, 0x50 },
    { 0x3053, 0x00 },
    { 0x3054, 0x03 },
    { 0x3055, 0xF7 },
    { 0x3056, 0xF8 },
    { 0x3057, 0x29 },
    { 0x3058, 0x1F },
    { 0x3059, 0x1E },
    { 0x3064, 0x00 },
    { 0x3065, 0x04 },
    { 0x3067, 0x00 },
    { 0x1000, 0x43 },
    { 0x1001, 0x43 },
    { 0x1002, 0x43 },
    { 0x0350, 0x7F },
    { 0x1006, 0x01 },
    { 0x1003, 0x00 },
    { 0x1008, 0x01 },
    { 0x1009, 0xA0 },
    { 0x100A, 0x60 },
    { 0x100B, 0x90 },
    { 0x100C, 0x40 },
    { 0x1012, 0x00 },
    { 0x2000, 0x07 },
    { 0x2003, 0x00 },
    { 0x2004, 0x1C },
    { 0x2007, 0x00 },
    { 0x2008, 0x58 },
    { 0x200B, 0x00 },
    { 0x200C, 0x7A },
    { 0x200F, 0x00 },
    { 0x2010, 0xB8 },
    { 0x2013, 0x00 },
    { 0x2014, 0x58 },
    { 0x2017, 0x00 },
    { 0x2018, 0x9B },
    { 0x2100, 0x01 },
    { 0x2101, 0x64 },
    { 0x2108, 0x04 },
    { 0x2109, 0x04 },
    { 0x210B, 0xC0 },
    { 0x0202, 0x01 },
    { 0x0203, 0x08 },
    { 0x0205, 0x00 },
    { 0x210D, 0x20 },
    { 0x020E, 0x01 },
    { 0x020F, 0x00 },
    { 0x0383, 0x01 },
    { 0x0387, 0x01 },
    { 0x0390, 0x00 },
    { 0x3011, 0x70 },
    { 0x3059, 0x02 },
    { 0x3060, 0x0A },
    { 0x0101, 0x00 },
};

static const CameraReg HM01B0_QVGA_TABLE[] =
{
    { 0x0383, 0x01 },
    { 0x0387, 0x01 },
    { 0x0390, 0x00 },
    { 0x3010, 0x01 },
    { 0x2105, 0x04 },
    { 0x2106, 0x0E },
    { 0x0340, 0x04 },
    { 0x0341, 0x10 },
    { 0x0342, 0x01 },
    { 0x0343, 0x78 },
    { 0x0202, 0x00 },
    { 0x0203, 0xBC },
};

bool Camera::readRegister(uint16_t reg, uint8_t &out_value)
{
    Wire.beginTransmission(CAM_I2C_ADDR);
    Wire.write(uint8_t(reg >> 8));
    Wire.write(uint8_t(reg & 0xFF));

    if (Wire.endTransmission(false) != 0)
    {
        return false;
    }

    uint8_t got = uint8_t(Wire.requestFrom(int(CAM_I2C_ADDR), 1));

    if (got != 1)
    {
        return false;
    }

    out_value = uint8_t(Wire.read());

    return true;
}

bool Camera::writeRegister(uint16_t reg, uint8_t value)
{
    Wire.beginTransmission(CAM_I2C_ADDR);
    Wire.write(uint8_t(reg >> 8));
    Wire.write(uint8_t(reg & 0xFF));
    Wire.write(value);

    if (Wire.endTransmission() != 0)
    {
        return false;
    }

    return true;
}

bool Camera::writeRegisterList(const CameraReg *p_list, size_t count)
{
    size_t i = 0;

    if (p_list == nullptr)
    {
        return false;
    }

    for (i = 0; i < count; i++)
    {
        if (writeRegister(p_list[i].reg, p_list[i].value) == false)
        {
            gLogger.error("Camera reg write FAILED reg:0x%04X", p_list[i].reg);
            return false;
        }
    }

    return true;
}

bool Camera::readModelId(uint16_t &out_id)
{
    uint8_t hi = 0;
    uint8_t lo = 0;

    if (readRegister(HM01B0_REG_MODEL_ID_H, hi) == false)
    {
        return false;
    }

    if (readRegister(HM01B0_REG_MODEL_ID_L, lo) == false)
    {
        return false;
    }

    out_id = uint16_t((hi << 8) | lo);

    return true;
}

bool Camera::enableTestPattern(bool enable, bool walking_ones)
{
    uint8_t value = HM01B0_TP_OFF;

    if (enable == true)
    {
        if (walking_ones == true)
        {
            value = HM01B0_TP_WALKING_ONES;
        }
        else
        {
            value = HM01B0_TP_COLOR_BAR;
        }
    }

    gLogger.info("Camera test pattern set 0x%02X ...", value);

    if (writeRegister(HM01B0_REG_TEST_PATTERN, value) == false)
    {
        gLogger.error("Camera test pattern FAILED");
        return false;
    }

    if (writeRegister(HM01B0_REG_GRP_PARAM_HOLD, HM01B0_GRP_HOLD) == false)
    {
        return false;
    }

    gLogger.info("Camera test pattern ok");

    return true;
}

bool Camera::setStreaming(bool enable)
{
    uint8_t mode = HM01B0_MODE_STANDBY;

    if (enable == true)
    {
        mode = HM01B0_MODE_STREAMING;
    }

    if (writeRegister(HM01B0_REG_MODE_SELECT, mode) == false)
    {
        gLogger.error("Camera mode select FAILED");
        return false;
    }

    return true;
}

bool Camera::configureSensor()
{
    uint16_t model_id = 0;

    Wire.begin(PIN_CAM_SDA, PIN_CAM_SCL, CAM_I2C_HZ);

    if (readModelId(model_id) == false)
    {
        gLogger.error("Camera not responding on I2C");
        return false;
    }

    if (model_id != HM01B0_MODEL_ID)
    {
        gLogger.error("Camera wrong model id:0x%04X", model_id);
        return false;
    }

    gLogger.info("Camera HM01B0 detected id:0x%04X", model_id);

    gLogger.info("Camera sensor reset ...");

    if (writeRegister(HM01B0_REG_SW_RESET, 0x01) == false)
    {
        gLogger.error("Camera sensor reset FAILED");
        return false;
    }

    delay(10);

    gLogger.info("Camera base register load ...");

    if (writeRegisterList(HM01B0_INIT_TABLE, sizeof(HM01B0_INIT_TABLE) / sizeof(HM01B0_INIT_TABLE[0])) == false)
    {
        return false;
    }

    gLogger.info("Camera QVGA window load ...");

    if (writeRegisterList(HM01B0_QVGA_TABLE, sizeof(HM01B0_QVGA_TABLE) / sizeof(HM01B0_QVGA_TABLE[0])) == false)
    {
        return false;
    }

    gLogger.info("Camera 4-bit mode set ...");

    if (writeRegister(HM01B0_REG_BIT_CONTROL, HM01B0_BITCTRL_4BIT) == false)
    {
        gLogger.error("Camera 4-bit mode FAILED");
        return false;
    }

    if (writeRegister(HM01B0_REG_GRP_PARAM_HOLD, HM01B0_GRP_HOLD) == false)
    {
        return false;
    }

    gLogger.info("Camera sensor config ok");

    return true;
}

bool Camera::startCapture()
{
    s_rawLen = CAM_RAW_ALLOC_BYTES;

    gLogger.info("Camera raw buffer alloc ...");

    s_pRaw[0] = (uint8_t *)heap_caps_aligned_alloc(CAM_RAW_ALLOC_ALIGN, s_rawLen, MALLOC_CAP_SPIRAM | MALLOC_CAP_DMA);
    s_pRaw[1] = (uint8_t *)heap_caps_aligned_alloc(CAM_RAW_ALLOC_ALIGN, s_rawLen, MALLOC_CAP_SPIRAM | MALLOC_CAP_DMA);

    if (s_pRaw[0] == nullptr)
    {
        gLogger.error("Camera raw buffer alloc FAILED");
        return false;
    }

    if (s_pRaw[1] == nullptr)
    {
        gLogger.error("Camera raw buffer alloc FAILED");
        return false;
    }

    esp_cam_ctlr_dvp_pin_config_t pin_cfg = {};

    pin_cfg.data_width = (cam_ctlr_data_width_t)8;
    pin_cfg.data_io[0] = (gpio_num_t)PIN_CAM_D0;
    pin_cfg.data_io[1] = (gpio_num_t)PIN_CAM_D1;
    pin_cfg.data_io[2] = (gpio_num_t)PIN_CAM_D2;
    pin_cfg.data_io[3] = (gpio_num_t)PIN_CAM_D3;
    pin_cfg.data_io[4] = GPIO_NUM_NC;
    pin_cfg.data_io[5] = GPIO_NUM_NC;
    pin_cfg.data_io[6] = GPIO_NUM_NC;
    pin_cfg.data_io[7] = GPIO_NUM_NC;
    pin_cfg.vsync_io = (gpio_num_t)PIN_CAM_VSYNC;
    pin_cfg.de_io = (gpio_num_t)PIN_CAM_HREF;
    pin_cfg.pclk_io = (gpio_num_t)PIN_CAM_PCLK;
    pin_cfg.xclk_io = GPIO_NUM_NC;

    esp_cam_ctlr_dvp_config_t dvp_cfg = {};

    dvp_cfg.ctlr_id = CAM_DVP_CTLR_ID;
    dvp_cfg.clk_src = CAM_CLK_SRC_DEFAULT;
    dvp_cfg.h_res = CAM_DVP_H_RES;
    dvp_cfg.v_res = CAM_DVP_V_RES;
    dvp_cfg.input_data_color_type = CAM_CTLR_COLOR_RAW8;
    dvp_cfg.cam_data_width = 8;
    dvp_cfg.dma_burst_size = CAM_DVP_DMA_BURST;
    dvp_cfg.bk_buffer_dis = 1;
    dvp_cfg.external_xtal = 1;
    dvp_cfg.pin = &pin_cfg;

    gLogger.info("Camera DVP controller ...");

    esp_err_t err = esp_cam_new_dvp_ctlr(&dvp_cfg, &s_camHandle);

    if (err != ESP_OK)
    {
        gLogger.error("Camera DVP controller FAILED err:0x%X", int(err));
        return false;
    }

    esp_cam_ctlr_evt_cbs_t cbs = {};

    cbs.on_get_new_trans = camOnGetNewTrans;
    cbs.on_trans_finished = camOnTransFinished;

    err = esp_cam_ctlr_register_event_callbacks(s_camHandle, &cbs, this);

    if (err != ESP_OK)
    {
        gLogger.error("Camera DVP callbacks FAILED err:0x%X", int(err));
        return false;
    }

    err = esp_cam_ctlr_enable(s_camHandle);

    if (err != ESP_OK)
    {
        gLogger.error("Camera DVP enable FAILED err:0x%X", int(err));
        return false;
    }

    err = esp_cam_ctlr_start(s_camHandle);

    if (err != ESP_OK)
    {
        gLogger.error("Camera DVP start FAILED err:0x%X", int(err));
        return false;
    }

    gLogger.info("Camera capture started");

    return true;
}

bool Camera::initialize()
{
    if (configureSensor() == false)
    {
        m_ready = false;
        return false;
    }

    m_ready = true;
    m_captureReady = false;

    if (startCapture() == false)
    {
        gLogger.error("Camera capture init FAILED, using gradient fallback");
        return true;
    }

    if (CAM_START_WITH_TEST_PATTERN == 1)
    {
        enableTestPattern(true, false);
    }

    if (setStreaming(true) == false)
    {
        gLogger.error("Camera streaming enable FAILED");
    }

    m_captureReady = true;

    return true;
}

bool Camera::isReady()
{
    return m_ready;
}

bool Camera::getCaptureStats(uint32_t &out_frames, size_t &out_last_len)
{
    out_frames = s_frameCount;
    out_last_len = s_latestLen;

    return true;
}

int Camera::getWidth()
{
    return CAM_WIDTH;
}

int Camera::getHeight()
{
    return CAM_HEIGHT;
}

bool Camera::reassembleNibbles(const uint8_t *p_raw, size_t raw_len, uint8_t *p_gray, size_t gray_len)
{
    int row = 0;
    int col = 0;

    if (p_raw == nullptr)
    {
        return false;
    }

    if (p_gray == nullptr)
    {
        return false;
    }

    if (raw_len < size_t(CAM_RAW_LINE_BYTES) * size_t(CAM_HEIGHT))
    {
        return false;
    }

    if (gray_len < size_t(CAM_WIDTH) * size_t(CAM_HEIGHT))
    {
        return false;
    }

    for (row = 0; row < CAM_HEIGHT; row++)
    {
        size_t raw_base = size_t(row) * size_t(CAM_RAW_LINE_BYTES);
        size_t gray_base = size_t(row) * size_t(CAM_WIDTH);

        for (col = 0; col < CAM_WIDTH; col++)
        {
            size_t pair = raw_base + (size_t(col) * 2);
            uint8_t first = uint8_t(p_raw[pair] & 0x0F);
            uint8_t second = uint8_t(p_raw[pair + 1] & 0x0F);

            p_gray[gray_base + size_t(col)] = uint8_t((second << 4) | first);
        }
    }

    return true;
}

bool Camera::captureRaw(const uint8_t *&out_p_raw, size_t &out_len)
{
    if (s_pRaw[0] == nullptr)
    {
        return false;
    }

    // Wait for a freshly completed frame. The driver fills the alternate buffer
    // while the ready buffer below stays frozen, and it will not cycle back to
    // the ready buffer for two more frame periods, far longer than reassembly
    // takes, so the read stays coherent without stopping the capture.
    s_frameReady = false;

    uint32_t start_ms = millis();

    while (s_frameReady == false)
    {
        if ((millis() - start_ms) > CAM_FRAME_TIMEOUT_MS)
        {
            return false;
        }

        delay(1);
    }

    int idx = s_readyIdx;

    if (idx < 0)
    {
        return false;
    }

    out_p_raw = s_pRaw[idx];
    out_len = s_latestLen;

    return true;
}

bool Camera::fillGradient(uint8_t *p_buffer, size_t buffer_len, size_t &out_len)
{
    size_t needed = size_t(CAM_WIDTH) * size_t(CAM_HEIGHT);
    int row = 0;
    int col = 0;

    if (p_buffer == nullptr)
    {
        return false;
    }

    if (buffer_len < needed)
    {
        return false;
    }

    for (row = 0; row < CAM_HEIGHT; row++)
    {
        for (col = 0; col < CAM_WIDTH; col++)
        {
            p_buffer[(row * CAM_WIDTH) + col] = uint8_t((col + m_testPhase) & 0xFF);
        }
    }

    m_testPhase = uint8_t(m_testPhase + 4);
    out_len = needed;

    return true;
}

bool Camera::captureFrame(uint8_t *p_buffer, size_t buffer_len, size_t &out_len)
{
    if (p_buffer == nullptr)
    {
        return false;
    }

    if (m_captureReady == false)
    {
        return fillGradient(p_buffer, buffer_len, out_len);
    }

    const uint8_t *p_raw = nullptr;
    size_t raw_len = 0;

    if (captureRaw(p_raw, raw_len) == false)
    {
        return fillGradient(p_buffer, buffer_len, out_len);
    }

    if (reassembleNibbles(p_raw, raw_len, p_buffer, buffer_len) == false)
    {
        return fillGradient(p_buffer, buffer_len, out_len);
    }

    out_len = size_t(CAM_WIDTH) * size_t(CAM_HEIGHT);

    return true;
}

bool Camera::encodeJpeg(const uint8_t *p_gray, size_t gray_len, uint8_t **pp_out, size_t &out_len)
{
    size_t jpeg_len = 0;

    if (pp_out == nullptr)
    {
        return false;
    }

    bool ok = fmt2jpg((uint8_t *)p_gray, gray_len, CAM_WIDTH, CAM_HEIGHT,
                      PIXFORMAT_GRAYSCALE, CAM_JPEG_QUALITY, pp_out, &jpeg_len);

    if (ok == false)
    {
        return false;
    }

    out_len = jpeg_len;

    return true;
}

bool Camera::freeJpeg(uint8_t *p_jpeg)
{
    if (p_jpeg == nullptr)
    {
        return false;
    }

    free(p_jpeg);

    return true;
}