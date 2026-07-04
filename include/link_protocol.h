#pragma once

#include <cstdint>

#define LINK_SOF             0xA5
#define LINK_MAX_PAYLOAD     64
#define LINK_PING_TIMEOUT_MS 1500

enum class LinkCommand : uint8_t
{
    PING        = 0x01,
    GET_RESULTS = 0x02,
    RUN_TEST    = 0x03,
    START_RUN   = 0x10,
    DRIVE       = 0x11,
    SET_IR      = 0x12,
    REBOOT      = 0x7E,
    ESTOP       = 0x7F
};

enum class LinkResponse : uint8_t
{
    PONG      = 0x81,
    RESULTS   = 0x82,
    TEST_DONE = 0x83,
    BOOTED    = 0x84,
    TELEMETRY = 0x85,
    ACK       = 0x8E,
    NACK      = 0x8F
};

inline uint8_t linkCrc8(const uint8_t *p_data, uint32_t length)
{
    uint8_t  crc = 0;
    uint32_t i   = 0;
    int      bit = 0;

    for (i = 0; i < length; i++)
    {
        crc = uint8_t(crc ^ p_data[i]);

        for (bit = 0; bit < 8; bit++)
        {
            if ((crc & 0x80) != 0)
            {
                crc = uint8_t((crc << 1) ^ 0x07);
            }
            else
            {
                crc = uint8_t(crc << 1);
            }
        }
    }

    return crc;
}