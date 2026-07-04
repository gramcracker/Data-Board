#include "stream_server.h"
#include "camera.h"
#include "camera_globals.h"
#include "link.h"
#include "status_codes.h"
#include "logger.h"
#include <Arduino.h>
#include <WiFi.h>
#include <cstring>

static WiFiServer gControlServer(80);
static WiFiServer gStreamServer(81);
static uint8_t    gFrameBuffer[CAM_FRAME_BYTES];

static const char INDEX_HTML[] = R"HTML(<!doctype html>
<html>
<head>
<meta name="viewport" content="width=device-width,initial-scale=1">
<title>Mo-chan test panel</title>
<style>
body{margin:0;background:#111;color:#eee;font-family:sans-serif;text-align:center}
button{margin:4px;padding:12px 16px;font-size:16px;border:0;border-radius:6px;background:#2a2a2a;color:#eee}
button:active{background:#0a84ff}
.pad button{width:70px}
#log{white-space:pre-wrap;text-align:left;margin:8px;padding:8px;background:#000;border-radius:6px;height:120px;overflow:auto;font-family:monospace;font-size:12px}
#tel{font-family:monospace;font-size:13px}
img{max-width:100%;height:auto;background:#000}
.row{margin:6px}
.estop{background:#b00020}
</style>
</head>
<body>
<div class="row">
<button onclick="cmd('/cmd/ping')">Ping</button>
<button onclick="cmd('/cmd/status')">Status</button>
<button onclick="cmd('/cmd/start')">Start Run</button>
<button onclick="cmd('/cmd/reboot')">Reboot STM</button>
<button class="estop" onclick="cmd('/cmd/estop')">E-STOP</button>
</div>
<div class="row pad">
<button onpointerdown="drive(200,0)" onpointerup="stop()" onpointerleave="stop()">Fwd</button><br>
<button onpointerdown="drive(0,200)" onpointerup="stop()" onpointerleave="stop()">Left</button>
<button onclick="stop()">Stop</button>
<button onpointerdown="drive(0,-200)" onpointerup="stop()" onpointerleave="stop()">Right</button><br>
<button onpointerdown="drive(-200,0)" onpointerup="stop()" onpointerleave="stop()">Back</button>
</div>
<img id="cam" src="">
<div class="row" id="tel">telemetry: (waiting)</div>
<div id="log"></div>
<script>
function log(t){var l=document.getElementById('log');l.textContent=(new Date().toLocaleTimeString()+"  "+t+"\n")+l.textContent;}
function cmd(p){fetch(p).then(function(r){return r.text();}).then(function(t){log(p+"  ->  "+t);}).catch(function(e){log(p+"  ERR "+e);});}
function drive(v,w){fetch('/cmd/drive?v='+v+'&w='+w).then(function(r){return r.text();}).then(function(t){log('drive '+v+','+w+"  ->  "+t);});}
function stop(){drive(0,0);}
function poll(){fetch('/cmd/telemetry').then(function(r){return r.json();}).then(function(d){if(d.ok==true){document.getElementById('tel').textContent='seq '+d.seq+'  state '+d.state+'  v '+d.v+'  w '+d.w+'  encL '+d.enc_l+'  encR '+d.enc_r;}}).catch(function(e){});}
document.getElementById('cam').src=location.protocol+'//'+location.hostname+':81/stream';
setInterval(poll,500);
</script>
</body>
</html>)HTML";

static bool sendJson(WiFiClient *p_wifi, const char *p_body)
{
    p_wifi->print("HTTP/1.1 200 OK\r\n");
    p_wifi->print("Content-Type: application/json\r\n");
    p_wifi->print("Connection: close\r\n\r\n");
    p_wifi->print(p_body);

    return true;
}

static long queryInt(const String &line, const char *p_key, long fallback)
{
    int idx = line.indexOf(p_key);

    if (idx < 0)
    {
        return fallback;
    }

    int start = idx + int(strlen(p_key));
    int end   = start;

    while (end < int(line.length()))
    {
        char c = line.charAt(end);

        if (((c >= '0') && (c <= '9')) || (c == '-'))
        {
            end++;
        }
        else
        {
            break;
        }
    }

    if (end == start)
    {
        return fallback;
    }

    String num = line.substring(start, end);

    return num.toInt();
}

bool StreamServer::initialize(Camera *p_camera, Link *p_link, uint16_t port)
{
    if (p_camera == nullptr)
    {
        return false;
    }

    m_pCamera = p_camera;
    m_pLink   = p_link;
    m_port    = port;

    return true;
}

bool StreamServer::start()
{
    gLogger.attempt("Stream server start");

    gControlServer = WiFiServer(m_port);
    gControlServer.begin();

    xTaskCreatePinnedToCore(streamTask, "stream", 8192, this, 1, nullptr, 1);

    gLogger.success("Stream server start");

    return true;
}

void StreamServer::streamTask(void *p_arg)
{
    StreamServer *p_self = (StreamServer *)p_arg;

    p_self->runStreamLoop();
}

void StreamServer::runStreamLoop()
{
    gStreamServer = WiFiServer(uint16_t(m_port + 1));
    gStreamServer.begin();

    while (true)
    {
        WiFiClient client = gStreamServer.accept();

        if (!client)
        {
            delay(10);
            continue;
        }

        client.setNoDelay(true);

        uint32_t start = millis();

        while ((client.available() == 0) && (client.connected() == true))
        {
            if ((millis() - start) > 1000)
            {
                break;
            }

            delay(1);
        }

        while (client.available() > 0)
        {
            String header = client.readStringUntil('\n');

            if (header.length() <= 1)
            {
                break;
            }
        }

        sendStream(&client);
        client.stop();
    }
}

bool StreamServer::sendIndexPage(void *p_client)
{
    WiFiClient *p_wifi = (WiFiClient *)p_client;

    p_wifi->print("HTTP/1.1 200 OK\r\n");
    p_wifi->print("Content-Type: text/html\r\n");
    p_wifi->print("Connection: close\r\n\r\n");
    p_wifi->print(INDEX_HTML);

    return true;
}

bool StreamServer::handlePing(void *p_client)
{
    WiFiClient *p_wifi = (WiFiClient *)p_client;
    bool        ok     = false;

    if (m_pLink != nullptr)
    {
        ok = m_pLink->ping(LINK_PING_TIMEOUT_MS);
    }

    if (ok == true)
    {
        sendJson(p_wifi, "{\"ok\":true,\"msg\":\"pong\"}");
    }
    else
    {
        sendJson(p_wifi, "{\"ok\":false,\"msg\":\"timeout\"}");
    }

    return true;
}

bool StreamServer::handleStatus(void *p_client)
{
    WiFiClient  *p_wifi  = (WiFiClient *)p_client;
    LinkResponse type    = LinkResponse::NACK;
    uint8_t      payload[LINK_MAX_PAYLOAD] = {0};
    uint8_t      length  = 0;
    bool         ok      = false;

    if (m_pLink != nullptr)
    {
        ok = m_pLink->command(LinkCommand::GET_RESULTS, nullptr, 0, type, payload, length, 500);
    }

    if ((ok == false) || (type != LinkResponse::RESULTS))
    {
        sendJson(p_wifi, "{\"ok\":false,\"results\":[]}");
        return true;
    }

    p_wifi->print("HTTP/1.1 200 OK\r\n");
    p_wifi->print("Content-Type: application/json\r\n");
    p_wifi->print("Connection: close\r\n\r\n");
    p_wifi->print("{\"ok\":true,\"results\":[");

    uint8_t count = uint8_t(length / 2);
    uint8_t i     = 0;

    for (i = 0; i < count; i++)
    {
        uint16_t raw = uint16_t(payload[i * 2]) | (uint16_t(payload[(i * 2) + 1]) << 8);

        StatusCode status;
        status.setRaw(raw);

        char text[48] = {0};
        statusToString(status, text, sizeof(text));

        if (i > 0)
        {
            p_wifi->print(",");
        }

        p_wifi->print("{\"ok\":");

        if (status.isOk() == true)
        {
            p_wifi->print("true");
        }
        else
        {
            p_wifi->print("false");
        }

        p_wifi->print(",\"text\":\"");
        p_wifi->print(text);
        p_wifi->print("\"}");
    }

    p_wifi->print("]}");

    return true;
}

bool StreamServer::handleTelemetry(void *p_client)
{
    WiFiClient   *p_wifi = (WiFiClient *)p_client;
    LinkTelemetry telemetry;
    bool          ok = false;

    if (m_pLink != nullptr)
    {
        ok = m_pLink->getTelemetry(telemetry);
    }

    if (ok == false)
    {
        sendJson(p_wifi, "{\"ok\":false}");
        return true;
    }

    p_wifi->print("HTTP/1.1 200 OK\r\n");
    p_wifi->print("Content-Type: application/json\r\n");
    p_wifi->print("Connection: close\r\n\r\n");
    p_wifi->print("{\"ok\":true");
    p_wifi->print(",\"seq\":");
    p_wifi->print(telemetry.seq);
    p_wifi->print(",\"t_ms\":");
    p_wifi->print(telemetry.t_ms);
    p_wifi->print(",\"state\":");
    p_wifi->print(telemetry.state);
    p_wifi->print(",\"v\":");
    p_wifi->print(telemetry.v);
    p_wifi->print(",\"w\":");
    p_wifi->print(telemetry.w);
    p_wifi->print(",\"enc_l\":");
    p_wifi->print(telemetry.enc_l);
    p_wifi->print(",\"enc_r\":");
    p_wifi->print(telemetry.enc_r);
    p_wifi->print("}");

    return true;
}

bool StreamServer::handleDrive(void *p_client, const String &request_line)
{
    WiFiClient *p_wifi = (WiFiClient *)p_client;
    int16_t     v      = int16_t(queryInt(request_line, "v=", 0));
    int16_t     w      = int16_t(queryInt(request_line, "w=", 0));
    uint8_t     payload[4] = {0};

    payload[0] = uint8_t(v & 0xFF);
    payload[1] = uint8_t((v >> 8) & 0xFF);
    payload[2] = uint8_t(w & 0xFF);
    payload[3] = uint8_t((w >> 8) & 0xFF);

    if (m_pLink != nullptr)
    {
        m_pLink->sendCommand(LinkCommand::DRIVE, payload, 4);
    }

    sendJson(p_wifi, "{\"ok\":true,\"msg\":\"drive sent\"}");

    return true;
}

bool StreamServer::handleSimpleCommand(void *p_client, LinkCommand command)
{
    WiFiClient  *p_wifi  = (WiFiClient *)p_client;
    LinkResponse type    = LinkResponse::NACK;
    uint8_t      payload[LINK_MAX_PAYLOAD] = {0};
    uint8_t      length  = 0;
    bool         ok      = false;

    if (m_pLink != nullptr)
    {
        ok = m_pLink->command(command, nullptr, 0, type, payload, length, 500);
    }

    if ((ok == true) && (type == LinkResponse::ACK))
    {
        sendJson(p_wifi, "{\"ok\":true,\"msg\":\"ack\"}");
    }
    else
    {
        sendJson(p_wifi, "{\"ok\":false,\"msg\":\"no ack\"}");
    }

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
    WiFiClient client = gControlServer.accept();

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

    if (request_line.indexOf("/cmd/ping") >= 0)
    {
        handlePing(&client);
    }
    else if (request_line.indexOf("/cmd/status") >= 0)
    {
        handleStatus(&client);
    }
    else if (request_line.indexOf("/cmd/telemetry") >= 0)
    {
        handleTelemetry(&client);
    }
    else if (request_line.indexOf("/cmd/start") >= 0)
    {
        handleSimpleCommand(&client, LinkCommand::START_RUN);
    }
    else if (request_line.indexOf("/cmd/reboot") >= 0)
    {
        handleSimpleCommand(&client, LinkCommand::REBOOT);
    }
    else if (request_line.indexOf("/cmd/estop") >= 0)
    {
        handleSimpleCommand(&client, LinkCommand::ESTOP);
    }
    else if (request_line.indexOf("/cmd/drive") >= 0)
    {
        handleDrive(&client, request_line);
    }
    else
    {
        sendIndexPage(&client);
    }

    client.stop();

    return true;
}