#pragma once

#include <stdint.h>
#include <stddef.h>
#include "driver/i2s_std.h"
#include "driver/gpio.h"

class Speaker
{
public:
    bool initialize();
    bool writeSamples(const int16_t *p_samples, size_t sample_count);
    bool enable();
    bool shutdown();

private:
    bool m_ready = false;
    i2s_chan_handle_t m_txHandle = nullptr;
};
