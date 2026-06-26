#pragma once

#include <cstdint>

class Arduino_DataBus;
class Arduino_GFX;

class Screen
{
public:
    bool initialize();
    bool clear(uint16_t color);
    bool printLine(const char *p_text);
    bool showBootBanner();
    bool runSelfTest();
    bool showStatus(const char *p_title, const char *p_line);

private:
    Arduino_DataBus *m_pBus = nullptr;
    Arduino_GFX     *m_pGfx = nullptr;
    int16_t          m_cursorY = 0;
};
