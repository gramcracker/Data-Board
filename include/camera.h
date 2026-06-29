#pragma once

#include <cstdint>
#include <cstddef>

class Camera
{
public:
    bool initialize();
    bool captureFrame(uint8_t *p_buffer, size_t buffer_len, size_t &out_len);
    bool encodeJpeg(const uint8_t *p_gray, size_t gray_len, uint8_t **pp_out, size_t &out_len);
    bool freeJpeg(uint8_t *p_jpeg);
    bool getCaptureStats(uint32_t &out_frames, size_t &out_last_len);

private:
    bool     m_ready = false;
    uint32_t m_frameCount = 0;
    size_t   m_lastLen = 0;
};