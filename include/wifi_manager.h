#pragma once

#include <cstdint>

class WifiManager
{
public:
    bool initialize();
    bool beginConnectOrProvision(const char *p_service_name, const char *p_pop);
    bool isConnected();
    bool isProvisioningActive();
    bool getIp(char *p_out, uint32_t out_len);
};
