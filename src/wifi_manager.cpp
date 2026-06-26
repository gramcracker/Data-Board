#include "wifi_manager.h"
#include "config.h"
#include "logger.h"
#include <Arduino.h>
#include <WiFi.h>
#include <WiFiProv.h>

static volatile bool gWifiConnected = false;
static volatile bool gProvisioningActive = false;

static void wifiProvEvent(arduino_event_t *p_event)
{
    if (p_event->event_id == ARDUINO_EVENT_WIFI_STA_GOT_IP)
    {
        gWifiConnected = true;
        gLogger.success("WiFi connect");
        return;
    }

    if (p_event->event_id == ARDUINO_EVENT_PROV_START)
    {
        gProvisioningActive = true;
        gLogger.attempt("Provisioning, waiting for app");
        return;
    }

    if (p_event->event_id == ARDUINO_EVENT_PROV_CRED_RECV)
    {
        gLogger.info("Provisioning credentials received");
        return;
    }

    if (p_event->event_id == ARDUINO_EVENT_PROV_CRED_SUCCESS)
    {
        // Core 3.x workaround: provisioning saves credentials but does not
        // start the STA connection on its own, so start it here.
        gLogger.attempt("WiFi connect after provisioning");
        WiFi.begin();
        return;
    }

    if (p_event->event_id == ARDUINO_EVENT_PROV_CRED_FAIL)
    {
        gLogger.failure("Provisioning credentials");
        return;
    }

    if (p_event->event_id == ARDUINO_EVENT_PROV_END)
    {
        gProvisioningActive = false;
        gLogger.success("Provisioning");
        return;
    }
}

bool WifiManager::initialize()
{
    gWifiConnected = false;
    gProvisioningActive = false;
    WiFi.onEvent(wifiProvEvent);

    return true;
}

bool WifiManager::beginConnectOrProvision(const char *p_service_name, const char *p_pop)
{
    bool force_reset = false;

    if (PROV_FORCE_RESET == 1)
    {
        force_reset = true;
    }

    gLogger.attempt("WiFi connect or provision");
    WiFiProv.beginProvision(NETWORK_PROV_SCHEME_BLE,
                            NETWORK_PROV_SCHEME_HANDLER_FREE_BTDM,
                            NETWORK_PROV_SECURITY_1,
                            p_pop,
                            p_service_name,
                            nullptr,
                            nullptr,
                            force_reset);

    return true;
}

bool WifiManager::isConnected()
{
    return gWifiConnected;
}

bool WifiManager::isProvisioningActive()
{
    return gProvisioningActive;
}

bool WifiManager::getIp(char *p_out, uint32_t out_len)
{
    if (p_out == nullptr)
    {
        return false;
    }

    String ip = WiFi.localIP().toString();
    snprintf(p_out, out_len, "%s", ip.c_str());

    return true;
}
