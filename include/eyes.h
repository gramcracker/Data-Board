#pragma once

#include "eyes_globals.h"
#include <cstdint>

class Arduino_GFX;

enum class EyeMood : uint8_t
{
    NEUTRAL = 0,
    HAPPY   = 1,
    TIRED   = 2,
    ANGRY   = 3
};

class Eyes
{
public:
    bool initialize(Arduino_GFX *p_gfx);
    void update(bool allow_draw);
    void setMood(EyeMood mood);
    void lookAt(int16_t norm_x, int16_t norm_y);
    void getState(uint8_t &out_mood, int16_t &out_gaze_x, int16_t &out_gaze_y, int16_t &out_open_pct);

private:
    void advanceState(uint32_t now);
    void draw();

    Arduino_GFX *m_pGfx = nullptr;
    bool         m_ready = false;
    bool         m_cleared = false;

    EyeMood      m_mood = EyeMood::NEUTRAL;

    uint32_t     m_lastDraw   = 0;
    uint32_t     m_lastBlink  = 0;
    uint32_t     m_blinkStart = 0;
    bool         m_blinking   = false;
    uint32_t     m_lastDrift  = 0;
    uint32_t     m_directedUntil = 0;

    int16_t      m_gazeX = 0;
    int16_t      m_gazeY = 0;
    int16_t      m_gazeTargetX = 0;
    int16_t      m_gazeTargetY = 0;

    int16_t      m_openPct = 100;

    uint32_t     m_lastEase = 0;

    bool         m_hasPrev = false;
    int16_t      m_prevLeftX = 0;
    int16_t      m_prevRightX = 0;
    int16_t      m_prevTopY = 0;
    int16_t      m_prevOpen = 0;
    uint8_t      m_prevMood = 0;
};