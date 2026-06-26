#include "logger.h"
#include <Arduino.h>
#include <cstdarg>
#include <cstdio>

Logger gLogger;

bool Logger::initialize(unsigned long baud)
{
    Serial.begin(baud);

    return true;
}

void Logger::writeLine(const char *p_text)
{
    Serial.println(p_text);
}

void Logger::info(const char *p_format, ...)
{
    va_list args;
    va_start(args, p_format);
    vsnprintf(m_buffer, sizeof(m_buffer), p_format, args);
    va_end(args);

    writeLine(m_buffer);
}

void Logger::error(const char *p_format, ...)
{
    char    prefixed[160] = {0};
    va_list args;

    va_start(args, p_format);
    vsnprintf(m_buffer, sizeof(m_buffer), p_format, args);
    va_end(args);

    snprintf(prefixed, sizeof(prefixed), "ERROR: %s", m_buffer);
    writeLine(prefixed);
}

void Logger::attempt(const char *p_what)
{
    info("%s ...", p_what);
}

void Logger::success(const char *p_what)
{
    info("%s ok", p_what);
}

void Logger::failure(const char *p_what)
{
    error("%s FAILED", p_what);
}
