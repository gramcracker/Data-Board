#pragma once

#include "controller_globals.h"
#include "controller_state_machine.h"
#include "screen.h"
#include "eyes.h"
#include "camera.h"
#include "wifi_manager.h"
#include "stream_server.h"
#include "link.h"
#include <cstdint>

class Controller
{
public:
    bool initialize();
    bool execute();

private:
    void handleBoot();
    void handleLinkCheck();
    void handleWifiConnect();
    void handleProvision();
    void handleCameraInit();
    void handleRun();
    void handleFault();

    ControllerStateMachine m_stateMachine;

    Screen       m_screen;
    Eyes         m_eyes;
    Camera       m_camera;
    WifiManager  m_wifi;
    StreamServer m_stream;
    Link         m_link;

    bool     m_linkOk = false;
    bool     m_cameraOk = false;
    bool     m_wifiOk = false;
    uint32_t m_lastRefresh = 0;
    char     m_ip[24] = {0};
};

extern Controller gController;
