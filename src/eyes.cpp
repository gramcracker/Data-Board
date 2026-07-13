#include "eyes.h"
#include <Arduino.h>
#include <Arduino_GFX_Library.h>

bool Eyes::initialize(Arduino_GFX *p_gfx)
{
    if (p_gfx == nullptr)
    {
        return false;
    }

    m_pGfx  = p_gfx;
    m_ready = true;

    return true;
}

void Eyes::setMood(EyeMood mood)
{
    m_mood = mood;
}

void Eyes::lookAt(int16_t norm_x, int16_t norm_y)
{
    if (norm_x > 100)
    {
        norm_x = 100;
    }

    if (norm_x < -100)
    {
        norm_x = -100;
    }

    if (norm_y > 100)
    {
        norm_y = 100;
    }

    if (norm_y < -100)
    {
        norm_y = -100;
    }

    m_gazeTargetX   = norm_x;
    m_gazeTargetY   = norm_y;
    m_directedUntil = millis() + EYE_LOOK_HOLD_MS;
}

void Eyes::getState(uint8_t &out_mood, int16_t &out_gaze_x, int16_t &out_gaze_y, int16_t &out_open_pct)
{
    out_mood     = uint8_t(m_mood);
    out_gaze_x   = m_gazeX;
    out_gaze_y   = m_gazeY;
    out_open_pct = m_openPct;
}

void Eyes::advanceState(uint32_t now)
{
    if (m_blinking == false)
    {
        if ((now - m_lastBlink) >= EYE_BLINK_INTERVAL_MS)
        {
            m_blinking   = true;
            m_blinkStart = now;
        }
    }

    m_openPct = 100;

    if (m_blinking == true)
    {
        uint32_t elapsed = now - m_blinkStart;

        if (elapsed >= EYE_BLINK_DURATION_MS)
        {
            m_blinking  = false;
            m_lastBlink = now;
            m_openPct   = 100;
        }
        else
        {
            float half = float(EYE_BLINK_DURATION_MS) * 0.5f;
            float frac = 0.0f;

            if (float(elapsed) < half)
            {
                frac = 1.0f - (float(elapsed) / half);
            }
            else
            {
                frac = (float(elapsed) - half) / half;
            }

            m_openPct = int16_t(frac * 100.0f);
        }
    }

    if (now > m_directedUntil)
    {
        if ((now - m_lastDrift) >= EYE_DRIFT_INTERVAL_MS)
        {
            m_lastDrift   = now;
            m_gazeTargetX = int16_t(random(-EYE_DRIFT_NORM, EYE_DRIFT_NORM + 1));
            m_gazeTargetY = int16_t(random(-EYE_DRIFT_NORM, EYE_DRIFT_NORM + 1));
        }
    }

    if ((now - m_lastEase) >= EYE_EASE_MS)
    {
        m_lastEase = now;
        m_gazeX += int16_t((m_gazeTargetX - m_gazeX) / 4);
        m_gazeY += int16_t((m_gazeTargetY - m_gazeY) / 4);
    }
}

void Eyes::update(bool allow_draw)
{
    if (m_ready == false)
    {
        return;
    }

    uint32_t now = millis();

    advanceState(now);

    if (allow_draw == false)
    {
        return;
    }

    if ((now - m_lastDraw) < EYE_REDRAW_MS)
    {
        return;
    }

    m_lastDraw = now;

    if (m_cleared == false)
    {
        m_pGfx->fillScreen(EYE_BG);
        m_cleared = true;
    }

    draw();
}

void Eyes::draw()
{
    int16_t gaze_x  = int16_t((int(m_gazeX) * EYE_GAZE_RANGE_PX) / 100);
    int16_t gaze_y  = int16_t((int(m_gazeY) * EYE_GAZE_RANGE_PX) / 100);
    int16_t open_px = int16_t((int(EYE_HEIGHT) * int(m_openPct)) / 100);
    int16_t y_shift = 0;

    if (m_mood == EyeMood::HAPPY)
    {
        open_px = int16_t((open_px * 6) / 10);
        y_shift = 12;
    }

    if (open_px < EYE_MIN_OPEN_PX)
    {
        open_px = EYE_MIN_OPEN_PX;
    }

    int16_t left_x  = (120 - (EYE_GAP / 2) - EYE_WIDTH) + gaze_x;
    int16_t right_x = (120 + (EYE_GAP / 2)) + gaze_x;
    int16_t top_y   = (EYE_CENTER_Y - (open_px / 2)) + gaze_y + y_shift;
    uint8_t mood    = uint8_t(m_mood);

    if (m_hasPrev == true)
    {
        if ((left_x == m_prevLeftX) && (top_y == m_prevTopY) && (open_px == m_prevOpen) && (mood == m_prevMood))
        {
            return;
        }

        m_pGfx->fillRect(m_prevLeftX,  m_prevTopY, EYE_WIDTH, m_prevOpen, EYE_BG);
        m_pGfx->fillRect(m_prevRightX, m_prevTopY, EYE_WIDTH, m_prevOpen, EYE_BG);
    }

    int16_t radius = EYE_RADIUS;

    if (radius > (open_px / 2))
    {
        radius = open_px / 2;
    }

    m_pGfx->fillRoundRect(left_x,  top_y, EYE_WIDTH, open_px, radius, EYE_COLOR);
    m_pGfx->fillRoundRect(right_x, top_y, EYE_WIDTH, open_px, radius, EYE_COLOR);

    if (m_mood == EyeMood::TIRED)
    {
        int16_t cover_h = int16_t((open_px * 4) / 10);
        m_pGfx->fillRect(left_x,  top_y, EYE_WIDTH, cover_h, EYE_BG);
        m_pGfx->fillRect(right_x, top_y, EYE_WIDTH, cover_h, EYE_BG);
    }

    if (m_mood == EyeMood::ANGRY)
    {
        int16_t brow = int16_t((open_px * 5) / 10);
        m_pGfx->fillTriangle(left_x + EYE_WIDTH, top_y, left_x + EYE_WIDTH, top_y + brow, left_x, top_y, EYE_BG);
        m_pGfx->fillTriangle(right_x, top_y, right_x, top_y + brow, right_x + EYE_WIDTH, top_y, EYE_BG);
    }

    m_prevLeftX  = left_x;
    m_prevRightX = right_x;
    m_prevTopY   = top_y;
    m_prevOpen   = open_px;
    m_prevMood   = mood;
    m_hasPrev    = true;
}