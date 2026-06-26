#pragma once

class Logger
{
public:
    bool initialize(unsigned long baud);
    void info(const char *p_format, ...);
    void error(const char *p_format, ...);
    void attempt(const char *p_what);
    void success(const char *p_what);
    void failure(const char *p_what);

private:
    void writeLine(const char *p_text);

    char m_buffer[160] = {0};
};

extern Logger gLogger;
