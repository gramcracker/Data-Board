#pragma once

#include "link_protocol.h"
#include <Arduino.h>
#include <cstdint>

class Camera;
class Link;

class StreamServer
{
public:
    bool initialize(Camera *p_camera, Link *p_link, uint16_t port);
    bool start();
    bool handleClients();
    bool isStreaming();

private:
    bool sendIndexPage(void *p_client);
    bool writeStreamHeader(void *p_client);
    bool pumpStream();

    bool handlePing(void *p_client);
    bool handleStatus(void *p_client);
    bool handleTelemetry(void *p_client);
    bool handleGetParams(void *p_client);
    bool handleSetParams(void *p_client, const String &request_line);
    bool handleDrive(void *p_client, const String &request_line);
    bool handleSimpleCommand(void *p_client, LinkCommand command);

    Camera  *m_pCamera = nullptr;
    Link    *m_pLink = nullptr;
    uint16_t m_port = 80;
};
