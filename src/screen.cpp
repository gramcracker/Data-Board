#include "screen.h"
#include "pins.h"
#include "logger.h"
#include <Arduino_GFX_Library.h>

#define SCREEN_TEXT_SIZE   2
#define SCREEN_LINE_HEIGHT 20
#define SCREEN_DIAMETER    240

bool Screen::initialize()
{
    m_pBus = new Arduino_ESP32SPI(PIN_TFT_DC, PIN_TFT_CS, PIN_TFT_SCK, PIN_TFT_MOSI, -1);

    if (m_pBus == nullptr)
    {
        return false;
    }

    m_pGfx = new Arduino_GC9A01(m_pBus, PIN_TFT_RST, 0, true);

    if (m_pGfx == nullptr)
    {
        return false;
    }

    if (m_pGfx->begin() == false)
    {
        return false;
    }

    pinMode(PIN_TFT_BL, OUTPUT);
    digitalWrite(PIN_TFT_BL, HIGH);

    m_pGfx->fillScreen(RGB565_BLACK);
    m_pGfx->setTextSize(SCREEN_TEXT_SIZE);
    m_cursorY = 0;

    return true;
}

bool Screen::clear(uint16_t color)
{
    if (m_pGfx == nullptr)
    {
        return false;
    }

    m_pGfx->fillScreen(color);
    m_cursorY = 0;

    return true;
}

bool Screen::printLine(const char *p_text)
{
    if (m_pGfx == nullptr)
    {
        return false;
    }

    m_pGfx->setCursor(10, m_cursorY);
    m_pGfx->setTextColor(RGB565_WHITE);
    m_pGfx->println(p_text);
    m_cursorY = int16_t(m_cursorY + SCREEN_LINE_HEIGHT);

    return true;
}

bool Screen::showBootBanner()
{
    clear(RGB565_BLACK);
    m_cursorY = 90;
    printLine("Mo-chan");
    printLine("booting");

    return true;
}

bool Screen::runSelfTest()
{
    if (m_pGfx == nullptr)
    {
        return false;
    }

    clear(RGB565_RED);
    delay(150);
    clear(RGB565_GREEN);
    delay(150);
    clear(RGB565_BLUE);
    delay(150);
    clear(RGB565_BLACK);

    return true;
}

bool Screen::showStatus(const char *p_title, const char *p_line)
{
    clear(RGB565_BLACK);
    m_cursorY = 90;
    printLine(p_title);
    printLine(p_line);

    return true;
}

Arduino_GFX *Screen::gfx()
{
    return m_pGfx;
}
