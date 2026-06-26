#pragma once

#include <cstdint>

class Camera;

class StreamServer
{
public:
    bool initialize(Camera *p_camera, uint16_t port);
    bool start();
    bool handleClients();

private:
    bool sendIndexPage(void *p_client);
    bool sendStream(void *p_client);

    Camera  *m_pCamera = nullptr;
    uint16_t m_port = 80;
};
