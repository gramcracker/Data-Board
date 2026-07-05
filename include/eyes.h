#pragma once

#include "eyes_globals.h"
#include <cstdint>

class Arduino_GFX;

class Eyes
{
public:
    bool initialize(Arduino_GFX *p_gfx);
    void update();
    void setGaze(int16_t target_x, int16_t target_y);

private:
    void draw(int16_t offset_x, int16_t offset_y, int16_t open_px);

    Arduino_GFX *m_pGfx = nullptr;
    bool         m_ready = false;

    uint32_t     m_lastDraw   = 0;
    uint32_t     m_lastBlink  = 0;
    uint32_t     m_blinkStart = 0;
    bool         m_blinking   = false;
    bool         m_cleared    = false;
    uint32_t     m_lastDrift  = 0;

    int16_t      m_gazeX = 0;
    int16_t      m_gazeY = 0;
    int16_t      m_gazeTargetX = 0;
    int16_t      m_gazeTargetY = 0;
};
