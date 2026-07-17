#include "controller.h"
#include "config.h"
#include "pins.h"
#include "globals.h"
#include "logger.h"
#include <Arduino.h>
#include <esp_heap_caps.h>

Controller gController;

bool Controller::initialize()
{
    gLogger.initialize(115200);

    if (m_screen.initialize() == false)
    {
        gLogger.error("Screen init failed");
    }

    m_screen.showBootBanner();
    m_eyes.initialize(m_screen.gfx());

    m_audioOk = m_soundSynth.initialize();

    if (m_audioOk == true)
    {
        m_audioOk = m_speaker.initialize();
    }

    if (m_audioOk == true)
    {
        // Output buffer lives in PSRAM alongside the synth scratch. An utterance
        // is rendered whole, then handed to I2S, so PSRAM latency does not
        // matter here and internal SRAM stays free for WiFi and the camera.
        m_pSoundBuf = (int16_t *)heap_caps_malloc(SYNTH_MAX_SAMPLES * sizeof(int16_t), MALLOC_CAP_SPIRAM);

        if (m_pSoundBuf == nullptr)
        {
            gLogger.failure("Controller sound buffer alloc");
            m_audioOk = false;
        }
    }

    if (m_audioOk == true)
    {
        m_speaker.enable();
    }

    if (m_audioOk == false)
    {
        gLogger.error("Audio init failed, continuing without sound");
    }

    m_stateMachine.initialize(ControllerState::BOOT);

    return true;
}

bool Controller::playEmote(Emote emote)
{
    if (m_audioOk == false)
    {
        return false;
    }

    size_t len = 0;

    // The seed advances every call so repeated playback is never identical. It
    // is reported alongside the emote so a host-side episodic log can
    // reconstruct exactly what the robot sounded like.
    m_soundSeed = m_soundSeed * 1664525u + 1013904223u;

    bool ok = m_soundSynth.render(emote, m_soundSeed, m_pSoundBuf, SYNTH_MAX_SAMPLES, len);

    if (ok == false)
    {
        gLogger.error("Controller emote render FAILED id:%u", (unsigned)emote);
        return false;
    }

    gLogger.info("Emote id:%u seed:%u samples:%u", (unsigned)emote, (unsigned)m_soundSeed, (unsigned)len);

    // i2s_channel_write blocks until the DMA ring drains, so this stalls the
    // loop for the length of the utterance. That is acceptable at boot but not
    // during RUN; see the note in the Audio Notion doc about moving playback to
    // its own task before Layer B is triggered from the host.
    return m_speaker.play(m_pSoundBuf, len);
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

    m_stream.initialize(&m_camera, &m_link, STREAM_PORT);
    m_stream.start();
    m_screen.showStatus("Streaming", m_ip);

    // Bring-up check: one emote at boot proves synth, I2S, and amp end to end.
    playEmote(Emote::REFUSE);

    m_stateMachine.setState(ControllerState::RUN);
}

void Controller::handleRun()
{
    m_stream.handleClients();
    m_link.poll();

    // The GC9A01 shares its GDMA channel with the camera. Drawing the face while
    // the camera streams corrupts the camera descriptors, so the eyes render only
    // when no stream client is connected, which is when the camera DMA is idle.
    if (m_stream.isStreaming() == true)
    {
        return;
    }

    m_eyes.update();
}

void Controller::handleFault()
{
    m_screen.showStatus("Fault", "check serial log");
    delay(1000);
}