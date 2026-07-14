#include "link.h"
#include "driver/gpio.h"
#include "logger.h"
#include <Arduino.h>

static uint16_t getU16(const uint8_t *p_buf, int &idx)
{
    uint16_t value = uint16_t(p_buf[idx]) | (uint16_t(p_buf[idx + 1]) << 8);
    idx += 2;
    return value;
}

static uint32_t getU32(const uint8_t *p_buf, int &idx)
{
    uint32_t value = uint32_t(p_buf[idx]) | (uint32_t(p_buf[idx + 1]) << 8) | (uint32_t(p_buf[idx + 2]) << 16) | (uint32_t(p_buf[idx + 3]) << 24);
    idx += 4;
    return value;
}

bool Link::initialize(unsigned long baud, int tx_pin, int rx_pin)
{
    Serial1.setRxBufferSize(1024);
    Serial1.begin(baud, SERIAL_8N1, rx_pin, tx_pin);
    gpio_pullup_en((gpio_num_t)rx_pin);

    return true;
}

bool Link::sendCommand(LinkCommand command, const uint8_t *p_payload, uint8_t length)
{
    uint8_t frame[4 + LINK_MAX_PAYLOAD] = {0};
    uint8_t i = 0;

    if (length > LINK_MAX_PAYLOAD)
    {
        return false;
    }

    frame[0] = LINK_SOF;
    frame[1] = uint8_t(command);
    frame[2] = length;

    for (i = 0; i < length; i++)
    {
        frame[3 + i] = p_payload[i];
    }

    frame[3 + length] = linkCrc8(&frame[1], uint32_t(length + 2));
    Serial1.write(frame, size_t(length + 4));

    return true;
}

bool Link::command(LinkCommand command, const uint8_t *p_payload, uint8_t length,
                   LinkResponse &out_type, uint8_t *p_out_payload, uint8_t &out_length,
                   uint32_t timeout_ms)
{
    if (sendCommand(command, p_payload, length) == false)
    {
        return false;
    }

    if (awaitResponse(out_type, p_out_payload, out_length, timeout_ms) == false)
    {
        return false;
    }

    return true;
}

bool Link::readResponse(LinkResponse &out_type, uint8_t *p_payload, uint8_t &out_length, uint32_t timeout_ms)
{
    uint32_t start = millis();

    while ((millis() - start) < timeout_ms)
    {
        if (Serial1.available() <= 0)
        {
            continue;
        }

        uint8_t sof = uint8_t(Serial1.read());

        if (sof != LINK_SOF)
        {
            continue;
        }

        while (Serial1.available() < 2)
        {
            if ((millis() - start) >= timeout_ms)
            {
                return false;
            }
        }

        uint8_t type_byte = uint8_t(Serial1.read());
        uint8_t length    = uint8_t(Serial1.read());

        if (length > LINK_MAX_PAYLOAD)
        {
            return false;
        }

        uint8_t i = 0;

        for (i = 0; i < length; i++)
        {
            while (Serial1.available() <= 0)
            {
                if ((millis() - start) >= timeout_ms)
                {
                    return false;
                }
            }

            p_payload[i] = uint8_t(Serial1.read());
        }

        while (Serial1.available() <= 0)
        {
            if ((millis() - start) >= timeout_ms)
            {
                return false;
            }
        }

        uint8_t crc_rx = uint8_t(Serial1.read());
        uint8_t check[2 + LINK_MAX_PAYLOAD] = {0};

        check[0] = type_byte;
        check[1] = length;

        for (i = 0; i < length; i++)
        {
            check[2 + i] = p_payload[i];
        }

        if (linkCrc8(check, uint32_t(length + 2)) != crc_rx)
        {
            return false;
        }

        out_type   = LinkResponse(type_byte);
        out_length = length;

        return true;
    }

    return false;
}

bool Link::awaitResponse(LinkResponse &out_type, uint8_t *p_payload, uint8_t &out_length, uint32_t timeout_ms)
{
    uint32_t start = millis();

    while (true)
    {
        uint32_t elapsed = millis() - start;

        if (elapsed >= timeout_ms)
        {
            return false;
        }

        if (readResponse(out_type, p_payload, out_length, timeout_ms - elapsed) == false)
        {
            return false;
        }

        if (out_type == LinkResponse::TELEMETRY)
        {
            parseTelemetry(p_payload, out_length);
            continue;
        }

        return true;
    }
}

bool Link::ping(uint32_t timeout_ms)
{
    LinkResponse response = LinkResponse::NACK;
    uint8_t      payload[LINK_MAX_PAYLOAD] = {0};
    uint8_t      length = 0;

    sendCommand(LinkCommand::PING, nullptr, 0);

    if (awaitResponse(response, payload, length, timeout_ms) == false)
    {
        return false;
    }

    if (response == LinkResponse::PONG)
    {
        return true;
    }

    if (response == LinkResponse::BOOTED)
    {
        return true;
    }

    return false;
}

bool Link::poll()
{
    LinkResponse response = LinkResponse::NACK;
    uint8_t      payload[LINK_MAX_PAYLOAD] = {0};
    uint8_t      length = 0;
    int          drained = 0;
    bool         got_any = false;

    while ((Serial1.available() > 0) && (drained < 16))
    {
        if (readResponse(response, payload, length, 5) == false)
        {
            break;
        }

        drained++;
        got_any = true;

        if (response == LinkResponse::TELEMETRY)
        {
            parseTelemetry(payload, length);
        }
        else
        {
            gLogger.info("Link response type:0x%02X len:%u", uint8_t(response), length);
        }
    }

    return got_any;
}

void Link::parseTelemetry(const uint8_t *p_payload, uint8_t length)
{
    int i = 0;

    if (length < 51)
    {
        return;
    }

    int idx = 0;

    m_telemetry.version = p_payload[idx];
    idx++;
    m_telemetry.seq = p_payload[idx];
    idx++;
    m_telemetry.t_ms = getU32(p_payload, idx);
    m_telemetry.state = p_payload[idx];
    idx++;
    m_telemetry.fault_flags = getU16(p_payload, idx);
    m_telemetry.enc_l = int32_t(getU32(p_payload, idx));
    m_telemetry.enc_r = int32_t(getU32(p_payload, idx));
    m_telemetry.x_mm = int16_t(getU16(p_payload, idx));
    m_telemetry.y_mm = int16_t(getU16(p_payload, idx));
    m_telemetry.theta_cdeg = int16_t(getU16(p_payload, idx));
    m_telemetry.v = int16_t(getU16(p_payload, idx));
    m_telemetry.w = int16_t(getU16(p_payload, idx));

    for (i = 0; i < 6; i++)
    {
        m_telemetry.imu[i] = int16_t(getU16(p_payload, idx));
    }

    for (i = 0; i < 4; i++)
    {
        m_telemetry.wall[i] = getU16(p_payload, idx);
    }

    for (i = 0; i < 2; i++)
    {
        m_telemetry.cliff[i] = getU16(p_payload, idx);
    }

    m_haveTelemetry = true;
}

bool Link::getTelemetry(LinkTelemetry &out_telemetry)
{
    if (m_haveTelemetry == false)
    {
        return false;
    }

    out_telemetry = m_telemetry;

    return true;
}