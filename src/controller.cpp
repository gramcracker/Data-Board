#include "controller.h"
#include "config.h"
#include "pins.h"
#include "globals.h"
#include "logger.h"
#include <Arduino.h>

Controller gController;

bool Controller::initialize()
{
    gLogger.initialize(115200);

    if (m_screen.initialize() == false)
    {
        gLogger.error("Screen init failed");
    }

    m_screen.showBootBanner();
    m_stateMachine.initialize(ControllerState::BOOT);

    return true;
}

bool Controller::execute()
{
    switch (m_stateMachine.getState())
    {
        case ControllerState::BOOT:
            handleBoot();
            break;

        case ControllerState::LINK_CHECK:
            handleLinkCheck();
            break;

        case ControllerState::WIFI_CONNECT:
            handleWifiConnect();
            break;

        case ControllerState::PROVISION:
            handleProvision();
            break;

        case ControllerState::CAMERA_INIT:
            handleCameraInit();
            break;

        case ControllerState::RUN:
            handleRun();
            break;

        case ControllerState::FAULT:
            handleFault();
            break;

        default:
            break;
    }

    return true;
}

void Controller::handleBoot()
{
    gLogger.info("Boot, running screen self-test");
    m_screen.runSelfTest();
    m_stateMachine.setState(ControllerState::LINK_CHECK);
}

void Controller::handleLinkCheck()
{
    m_link.initialize(LINK_BAUD, PIN_LINK_TX, PIN_LINK_RX);
    m_linkOk = m_link.ping(LINK_PING_TIMEOUT_MS);

    if (m_linkOk == true)
    {
        gLogger.info("STM32 link ok");
        m_screen.showStatus("Link", "STM32 ok");
    }

    if (m_linkOk == false)
    {
        gLogger.error("STM32 link failed");
        m_screen.showStatus("Link", "STM32 FAIL");
    }

    m_stateMachine.setState(ControllerState::WIFI_CONNECT);
}

void Controller::handleWifiConnect()
{
    if (m_stateMachine.isNewState() == true)
    {
        m_wifi.initialize();
        m_screen.showStatus("WiFi", "connecting");
        m_wifi.beginConnectOrProvision(PROV_SERVICE_NAME, PROV_POP);
    }

    if (m_wifi.isConnected() == true)
    {
        m_wifi.getIp(m_ip, sizeof(m_ip));
        gLogger.info("WiFi ok ip:%s", m_ip);
        m_screen.showStatus("WiFi", m_ip);
        m_stateMachine.setState(ControllerState::CAMERA_INIT);
        return;
    }

    if (m_wifi.isProvisioningActive() == true)
    {
        m_stateMachine.setState(ControllerState::PROVISION);
        return;
    }
}

void Controller::handleProvision()
{
    if (m_stateMachine.isNewState() == true)
    {
        gLogger.info("Open the Mo-chan app to set up WiFi");
        gLogger.info("BLE name:%s pin:%s", PROV_SERVICE_NAME, PROV_POP);
        m_screen.showStatus("Setup", "open app");
    }

    if (m_wifi.isConnected() == true)
    {
        m_wifi.getIp(m_ip, sizeof(m_ip));
        gLogger.info("Provisioned, WiFi ok ip:%s", m_ip);
        m_screen.showStatus("WiFi", m_ip);
        m_stateMachine.setState(ControllerState::CAMERA_INIT);
    }
}

void Controller::handleCameraInit()
{
    m_cameraOk = m_camera.initialize();

    if (m_cameraOk == false)
    {
        gLogger.error("Camera init failed");
        m_screen.showStatus("Camera", "FAIL");
    }

    if (m_cameraOk == true)
    {
        gLogger.info("Camera ready");
        m_screen.showStatus("Camera", "ready");
    }

    m_stream.initialize(&m_camera, STREAM_PORT);
    m_stream.start();
    m_stateMachine.setState(ControllerState::RUN);
}

void Controller::handleRun()
{
    m_stream.handleClients();
    m_link.poll();

    if ((millis() - m_lastRefresh) < DISPLAY_REFRESH_MS)
    {
        return;
    }

    m_lastRefresh = millis();
    m_screen.showStatus("Streaming", m_ip);

    uint32_t cam_frames = 0;
    size_t   cam_last_len = 0;

    m_camera.getCaptureStats(cam_frames, cam_last_len);
    gLogger.info("Camera frames:%u last_len:%u", cam_frames, (unsigned)cam_last_len);
}

void Controller::handleFault()
{
    m_screen.showStatus("Fault", "check serial log");
    delay(1000);
}