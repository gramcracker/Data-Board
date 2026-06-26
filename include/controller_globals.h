#pragma once

enum class ControllerState
{
    BOOT,
    LINK_CHECK,
    WIFI_CONNECT,
    PROVISION,
    CAMERA_INIT,
    RUN,
    FAULT
};

#define STATUS_TEXT_LEN          48
#define DISPLAY_REFRESH_MS       1000
