#include "stream_server.h"
#include "camera.h"
#include "camera_globals.h"
#include "logger.h"
#include <Arduino.h>
#include <WiFi.h>

static WiFiServer gWifiServer(80);
static uint8_t    gFrameBuffer[CAM_FRAME_BYTES];

bool StreamServer::initialize(Camera *p_camera, uint16_t port)
{
    if (p_camera == nullptr)
    {
        return false;
    }

    m_pCamera = p_camera;
    m_port = port;

    return true;
}

bool StreamServer::start()
{
    gLogger.attempt("Stream server start");
    gWifiServer = WiFiServer(m_port);
    gWifiServer.begin();
    gLogger.success("Stream server start");

    return true;
}

bool StreamServer::sendIndexPage(void *p_client)
{
    WiFiClient *p_wifi = (WiFiClient *)p_client;

    p_wifi->print("HTTP/1.1 200 OK\r\n");
    p_wifi->print("Content-Type: text/html\r\n");
    p_wifi->print("Connection: close\r\n");
    p_wifi->print("\r\n");
    p_wifi->print("<html><body style='margin:0;background:#000;text-align:center'>");
    p_wifi->print("<img src='/stream' style='max-width:100%;height:auto'>");
    p_wifi->print("</body></html>");

    return true;
}

bool StreamServer::sendStream(void *p_client)
{
    WiFiClient *p_wifi = (WiFiClient *)p_client;
    size_t      frame_len = 0;
    uint8_t    *p_jpeg = nullptr;
    size_t      jpeg_len = 0;

    p_wifi->print("HTTP/1.1 200 OK\r\n");
    p_wifi->print("Content-Type: multipart/x-mixed-replace; boundary=frame\r\n");
    p_wifi->print("Cache-Control: no-cache\r\n");
    p_wifi->print("Connection: close\r\n");
    p_wifi->print("\r\n");

    while (p_wifi->connected() == true)
    {
        if (m_pCamera->captureFrame(gFrameBuffer, sizeof(gFrameBuffer), frame_len) == false)
        {
            break;
        }

        if (m_pCamera->encodeJpeg(gFrameBuffer, frame_len, &p_jpeg, jpeg_len) == false)
        {
            break;
        }

        p_wifi->print("--frame\r\n");
        p_wifi->print("Content-Type: image/jpeg\r\n");
        p_wifi->print("Content-Length: ");
        p_wifi->print(jpeg_len);
        p_wifi->print("\r\n\r\n");
        p_wifi->write(p_jpeg, jpeg_len);
        p_wifi->print("\r\n");

        m_pCamera->freeJpeg(p_jpeg);
        p_jpeg = nullptr;

        delay(50);
    }

    return true;
}

bool StreamServer::handleClients()
{
    WiFiClient client = gWifiServer.accept();

    if (!client)
    {
        return false;
    }

    client.setNoDelay(true);

    uint32_t start = millis();

    while ((client.available() == 0) && (client.connected() == true))
    {
        if ((millis() - start) > 1000)
        {
            client.stop();
            return false;
        }

        delay(1);
    }

    String request_line = client.readStringUntil('\n');

    while (client.available() > 0)
    {
        String header = client.readStringUntil('\n');

        if (header.length() <= 1)
        {
            break;
        }
    }

    if (request_line.indexOf("/stream") >= 0)
    {
        sendStream(&client);
    }
    else
    {
        sendIndexPage(&client);
    }

    client.stop();

    return true;
}