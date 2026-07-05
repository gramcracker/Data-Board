#pragma once

#include "link_protocol.h"
#include <cstdint>

struct LinkTelemetry
{
    uint8_t  version;
    uint8_t  seq;
    uint32_t t_ms;
    uint8_t  state;
    uint16_t fault_flags;
    int32_t  enc_l;
    int32_t  enc_r;
    int16_t  x_mm;
    int16_t  y_mm;
    int16_t  theta_cdeg;
    int16_t  v;
    int16_t  w;
    int16_t  imu[6];
    uint16_t wall[4];
    uint16_t cliff[2];
};

class Link
{
public:
    bool initialize(unsigned long baud, int tx_pin, int rx_pin);
    bool ping(uint32_t timeout_ms);
    bool poll();
    bool getTelemetry(LinkTelemetry &out_telemetry);
    bool sendCommand(LinkCommand command, const uint8_t *p_payload, uint8_t length);
    bool command(LinkCommand command, const uint8_t *p_payload, uint8_t length,
                 LinkResponse &out_type, uint8_t *p_out_payload, uint8_t &out_length,
                 uint32_t timeout_ms);

private:
    bool readResponse(LinkResponse &out_type, uint8_t *p_payload, uint8_t &out_length, uint32_t timeout_ms);
    bool awaitResponse(LinkResponse &out_type, uint8_t *p_payload, uint8_t &out_length, uint32_t timeout_ms);
    void parseTelemetry(const uint8_t *p_payload, uint8_t length);

    LinkTelemetry m_telemetry = {};
    bool          m_haveTelemetry = false;
};