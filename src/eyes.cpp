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

void Eyes::setGaze(int16_t target_x, int16_t target_y)
{
    m_gazeTargetX = target_x;
    m_gazeTargetY = target_y;
}

void Eyes::update()
{
    if (m_ready == false)
    {
        return;
    }

    uint32_t now = millis();

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

    int16_t open_px = EYE_HEIGHT;

    if (m_blinking == false)
    {
        if ((now - m_lastBlink) >= EYE_BLINK_INTERVAL_MS)
        {
            m_blinking   = true;
            m_blinkStart = now;
        }
    }

    if (m_blinking == true)
    {
        uint32_t elapsed = now - m_blinkStart;

        if (elapsed >= EYE_BLINK_DURATION_MS)
        {
            m_blinking  = false;
            m_lastBlink = now;
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

            open_px = int16_t(float(EYE_HEIGHT) * frac);

            if (open_px < EYE_MIN_OPEN_PX)
            {
                open_px = EYE_MIN_OPEN_PX;
            }
        }
    }

    if ((now - m_lastDrift) >= EYE_DRIFT_INTERVAL_MS)
    {
        m_lastDrift   = now;
        m_gazeTargetX = int16_t(random(-EYE_DRIFT_PX, EYE_DRIFT_PX + 1));
        m_gazeTargetY = int16_t(random(-EYE_DRIFT_PX, EYE_DRIFT_PX + 1));
    }

    m_gazeX += int16_t((m_gazeTargetX - m_gazeX) / 4);
    m_gazeY += int16_t((m_gazeTargetY - m_gazeY) / 4);

    draw(m_gazeX, m_gazeY, open_px);
}

void Eyes::draw(int16_t offset_x, int16_t offset_y, int16_t open_px)
{
    int16_t band_x = 24;
    int16_t band_y = EYE_CENTER_Y - (EYE_HEIGHT / 2) - EYE_DRIFT_PX - 4;
    int16_t band_w = 240 - 48;
    int16_t band_h = EYE_HEIGHT + (2 * EYE_DRIFT_PX) + 8;

    m_pGfx->fillRect(band_x, band_y, band_w, band_h, EYE_BG);

    int16_t radius = EYE_RADIUS;

    if (radius > (open_px / 2))
    {
        radius = open_px / 2;
    }

    int16_t left_x  = (120 - (EYE_GAP / 2) - EYE_WIDTH) + offset_x;
    int16_t right_x = (120 + (EYE_GAP / 2)) + offset_x;
    int16_t top_y   = (EYE_CENTER_Y - (open_px / 2)) + offset_y;

    m_pGfx->fillRoundRect(left_x,  top_y, EYE_WIDTH, open_px, radius, EYE_COLOR);
    m_pGfx->fillRoundRect(right_x, top_y, EYE_WIDTH, open_px, radius, EYE_COLOR);
}
