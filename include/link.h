#pragma once

#include "link_protocol.h"
#include <cstdint>

class Link
{
public:
    bool initialize(unsigned long baud, int tx_pin, int rx_pin);
    bool ping(uint32_t timeout_ms);
    bool poll();
    bool sendCommand(LinkCommand command, const uint8_t *p_payload, uint8_t length);
    bool command(LinkCommand command, const uint8_t *p_payload, uint8_t length,
                 LinkResponse &out_type, uint8_t *p_out_payload, uint8_t &out_length,
                 uint32_t timeout_ms);

private:
    bool readResponse(LinkResponse &out_type, uint8_t *p_payload, uint8_t &out_length, uint32_t timeout_ms);
};
