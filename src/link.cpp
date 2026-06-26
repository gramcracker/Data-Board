#include "link.h"
#include "logger.h"
#include <Arduino.h>

bool Link::initialize(unsigned long baud, int tx_pin, int rx_pin)
{
    Serial1.begin(baud, SERIAL_8N1, rx_pin, tx_pin);

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

        out_type   = LinkResponse(Serial1.read());
        out_length = uint8_t(Serial1.read());

        uint8_t i = 0;

        for (i = 0; i < out_length; i++)
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

        Serial1.read();

        return true;
    }

    return false;
}

bool Link::ping(uint32_t timeout_ms)
{
    LinkResponse response = LinkResponse::NACK;
    uint8_t      payload[LINK_MAX_PAYLOAD] = {0};
    uint8_t      length = 0;

    sendCommand(LinkCommand::PING, nullptr, 0);

    if (readResponse(response, payload, length, timeout_ms) == false)
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

    if (Serial1.available() <= 0)
    {
        return false;
    }

    if (readResponse(response, payload, length, 50) == false)
    {
        return false;
    }

    gLogger.info("Link response type:0x%02X len:%u", uint8_t(response), length);

    return true;
}
