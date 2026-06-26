#pragma once

#include <cstdint>
#include <cstddef>

struct CameraReg
{
    uint16_t reg;
    uint8_t  value;
};

class Camera
{
public:
    bool initialize();
    bool isReady();
    bool readModelId(uint16_t &out_id);
    bool enableTestPattern(bool enable, bool walking_ones);
    bool setStreaming(bool enable);
    bool getCaptureStats(uint32_t &out_frames, size_t &out_last_len);
    bool captureFrame(uint8_t *p_buffer, size_t buffer_len, size_t &out_len);
    bool encodeJpeg(const uint8_t *p_gray, size_t gray_len, uint8_t **pp_out, size_t &out_len);
    bool freeJpeg(uint8_t *p_jpeg);
    int  getWidth();
    int  getHeight();

private:
    bool configureSensor();
    bool startCapture();
    bool writeRegisterList(const CameraReg *p_list, size_t count);
    bool readRegister(uint16_t reg, uint8_t &out_value);
    bool writeRegister(uint16_t reg, uint8_t value);
    bool captureRaw(const uint8_t *&out_p_raw, size_t &out_len);
    bool reassembleNibbles(const uint8_t *p_raw, size_t raw_len, uint8_t *p_gray, size_t gray_len);
    bool fillGradient(uint8_t *p_buffer, size_t buffer_len, size_t &out_len);

    bool     m_ready = false;
    bool     m_captureReady = false;
    uint8_t  m_testPhase = 0;
};