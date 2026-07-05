#include "stream_server.h"
#include "camera.h"
#include "camera_globals.h"
#include "link.h"
#include "status_codes.h"
#include "logger.h"
#include <Arduino.h>
#include <WiFi.h>
#include <cstring>

#define STREAM_FRAME_MS   50

static WiFiServer gControlServer(80);
static WiFiClient s_streamClient;
static uint32_t   s_lastFrameMs = 0;
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
#tel{white-space:pre;text-align:left;margin:8px auto;padding:8px;background:#000;border-radius:6px;max-width:320px;font-family:monospace;font-size:13px;line-height:1.5}
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
<button onclick="toggleStream()">View Camera</button>
</div>
<div class="row pad">
<button onpointerdown="drive(200,0)" onpointerup="stop()" onpointerleave="stop()">Fwd</button><br>
<button onpointerdown="drive(0,200)" onpointerup="stop()" onpointerleave="stop()">Left</button>
<button onclick="stop()">Stop</button>
<button onpointerdown="drive(0,-200)" onpointerup="stop()" onpointerleave="stop()">Right</button><br>
<button onpointerdown="drive(-200,0)" onpointerup="stop()" onpointerleave="stop()">Back</button>
</div>
<div class="row">
<button onclick="loadParams()">Load Params</button>
<button onclick="applyParams()">Apply Params</button>
</div>
<div class="row">
mm/count <input id="p_mmpc" size="8"> track mm <input id="p_track" size="6"><br>
Kp <input id="p_kp" size="5"> Ki <input id="p_ki" size="5"> Kd <input id="p_kd" size="5">
</div>
<img id="cam" src="">
<div id="tel">telemetry: (waiting)</div>
<div id="log"></div>
<script>
function log(t){var l=document.getElementById('log');l.textContent=(new Date().toLocaleTimeString()+"  "+t+"\n")+l.textContent;}
function cmd(p){fetch(p).then(function(r){return r.text();}).then(function(t){log(p+"  ->  "+t);}).catch(function(e){log(p+"  ERR "+e);});}
function drive(v,w){fetch('/cmd/drive?v='+v+'&w='+w).then(function(r){return r.text();}).then(function(t){log('drive '+v+','+w+"  ->  "+t);});}
function stop(){drive(0,0);}
function poll(){fetch('/cmd/telemetry').then(function(r){return r.json();}).then(function(d){
if(d.ok!=true){return;}
var states=['Booting','Self-test','Debug','Running','Fault'];
var st=states[d.state];if(st==undefined){st='state '+d.state;}
var status='OK';if(d.fault!=0){status='fault 0x'+d.fault.toString(16);}
var L=[];
L.push('State: '+st);
L.push('Status: '+status);
L.push('Linear velocity: '+d.v+' mm/s');
L.push('Angular velocity: '+d.w+' mrad/s');
L.push('Encoder Left: '+d.enc_l+' counts');
L.push('Encoder Right: '+d.enc_r+' counts');
L.push('X position: '+d.x+' mm');
L.push('Y position: '+d.y+' mm');
L.push('Heading: '+(d.th/100).toFixed(1)+' deg');
L.push('Acceleration X: '+(d.ax/1000).toFixed(2)+' g');
L.push('Acceleration Y: '+(d.ay/1000).toFixed(2)+' g');
L.push('Acceleration Z: '+(d.az/1000).toFixed(2)+' g');
L.push('Rotation X: '+(d.gx/10).toFixed(1)+' deg/s');
L.push('Rotation Y: '+(d.gy/10).toFixed(1)+' deg/s');
L.push('Rotation Z: '+(d.gz/10).toFixed(1)+' deg/s');
L.push('Wall sensor 1: '+d.w0+' adc');
L.push('Wall sensor 2: '+d.w1+' adc');
L.push('Wall sensor 3: '+d.w2+' adc');
L.push('Wall sensor 4: '+d.w3+' adc');
L.push('Cliff sensor: '+d.cliff+' adc');
document.getElementById('tel').textContent=L.join('\n');
}).catch(function(e){});}
function loadParams(){fetch('/cmd/getparams').then(function(r){return r.json();}).then(function(d){if(d.ok!=true){log('getparams failed');return;}document.getElementById('p_mmpc').value=d.mmpc;document.getElementById('p_track').value=d.track;document.getElementById('p_kp').value=d.kp;document.getElementById('p_ki').value=d.ki;document.getElementById('p_kd').value=d.kd;log('params loaded');});}
function applyParams(){var q='/cmd/setparams?mmpc='+document.getElementById('p_mmpc').value+'&track='+document.getElementById('p_track').value+'&kp='+document.getElementById('p_kp').value+'&ki='+document.getElementById('p_ki').value+'&kd='+document.getElementById('p_kd').value;fetch(q).then(function(r){return r.text();}).then(function(t){log('setparams -> '+t);});}
var streaming=false;
function toggleStream(){var c=document.getElementById('cam');if(streaming==false){c.src='/stream';streaming=true;log('camera on (face paused)');}else{c.src='';streaming=false;log('camera off (face resumes)');}}
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

static float queryFloat(const String &line, const char *p_key, float fallback)
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

        if (((c >= '0') && (c <= '9')) || (c == '-') || (c == '.'))
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

    return line.substring(start, end).toFloat();
}

static void putFloatLE(uint8_t *p_buf, int idx, float value)
{
    uint32_t bits = 0;
    memcpy(&bits, &value, sizeof(bits));
    p_buf[idx + 0] = uint8_t(bits & 0xFF);
    p_buf[idx + 1] = uint8_t((bits >> 8) & 0xFF);
    p_buf[idx + 2] = uint8_t((bits >> 16) & 0xFF);
    p_buf[idx + 3] = uint8_t((bits >> 24) & 0xFF);
}

static float getFloatLE(const uint8_t *p_buf, int idx)
{
    uint32_t bits = uint32_t(p_buf[idx]) | (uint32_t(p_buf[idx + 1]) << 8) | (uint32_t(p_buf[idx + 2]) << 16) | (uint32_t(p_buf[idx + 3]) << 24);

    float value = 0.0f;
    memcpy(&value, &bits, sizeof(value));

    return value;
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

    gLogger.success("Stream server start");

    return true;
}

bool StreamServer::isStreaming()
{
    return s_streamClient.connected();
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

bool StreamServer::writeStreamHeader(void *p_client)
{
    WiFiClient *p_wifi = (WiFiClient *)p_client;

    p_wifi->print("HTTP/1.1 200 OK\r\n");
    p_wifi->print("Content-Type: multipart/x-mixed-replace; boundary=frame\r\n");
    p_wifi->print("Cache-Control: no-cache\r\n");
    p_wifi->print("Connection: close\r\n");
    p_wifi->print("\r\n");

    return true;
}

bool StreamServer::pumpStream()
{
    size_t   frame_len = 0;
    uint8_t *p_jpeg = nullptr;
    size_t   jpeg_len = 0;

    if (s_streamClient.connected() == false)
    {
        return false;
    }

    if ((millis() - s_lastFrameMs) < STREAM_FRAME_MS)
    {
        return true;
    }

    s_lastFrameMs = millis();

    if (m_pCamera->captureFrame(gFrameBuffer, sizeof(gFrameBuffer), frame_len) == false)
    {
        return false;
    }

    if (m_pCamera->encodeJpeg(gFrameBuffer, frame_len, &p_jpeg, jpeg_len) == false)
    {
        return false;
    }

    s_streamClient.print("--frame\r\n");
    s_streamClient.print("Content-Type: image/jpeg\r\n");
    s_streamClient.print("Content-Length: ");
    s_streamClient.print(jpeg_len);
    s_streamClient.print("\r\n\r\n");
    s_streamClient.write(p_jpeg, jpeg_len);
    s_streamClient.print("\r\n");

    m_pCamera->freeJpeg(p_jpeg);

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
    p_wifi->print(",\"state\":");
    p_wifi->print(telemetry.state);
    p_wifi->print(",\"fault\":");
    p_wifi->print(telemetry.fault_flags);
    p_wifi->print(",\"v\":");
    p_wifi->print(telemetry.v);
    p_wifi->print(",\"w\":");
    p_wifi->print(telemetry.w);
    p_wifi->print(",\"enc_l\":");
    p_wifi->print(telemetry.enc_l);
    p_wifi->print(",\"enc_r\":");
    p_wifi->print(telemetry.enc_r);
    p_wifi->print(",\"x\":");
    p_wifi->print(telemetry.x_mm);
    p_wifi->print(",\"y\":");
    p_wifi->print(telemetry.y_mm);
    p_wifi->print(",\"th\":");
    p_wifi->print(telemetry.theta_cdeg);
    p_wifi->print(",\"ax\":");
    p_wifi->print(telemetry.imu[0]);
    p_wifi->print(",\"ay\":");
    p_wifi->print(telemetry.imu[1]);
    p_wifi->print(",\"az\":");
    p_wifi->print(telemetry.imu[2]);
    p_wifi->print(",\"gx\":");
    p_wifi->print(telemetry.imu[3]);
    p_wifi->print(",\"gy\":");
    p_wifi->print(telemetry.imu[4]);
    p_wifi->print(",\"gz\":");
    p_wifi->print(telemetry.imu[5]);
    p_wifi->print(",\"w0\":");
    p_wifi->print(telemetry.wall[0]);
    p_wifi->print(",\"w1\":");
    p_wifi->print(telemetry.wall[1]);
    p_wifi->print(",\"w2\":");
    p_wifi->print(telemetry.wall[2]);
    p_wifi->print(",\"w3\":");
    p_wifi->print(telemetry.wall[3]);
    p_wifi->print(",\"cliff\":");
    p_wifi->print(telemetry.cliff[0]);
    p_wifi->print("}");

    return true;
}

bool StreamServer::handleGetParams(void *p_client)
{
    WiFiClient  *p_wifi = (WiFiClient *)p_client;
    LinkResponse type   = LinkResponse::NACK;
    uint8_t      payload[LINK_MAX_PAYLOAD] = {0};
    uint8_t      length = 0;
    bool         ok     = false;

    if (m_pLink != nullptr)
    {
        ok = m_pLink->command(LinkCommand::GET_PARAMS, nullptr, 0, type, payload, length, 500);
    }

    if ((ok == false) || (type != LinkResponse::PARAMS) || (length < 20))
    {
        sendJson(p_wifi, "{\"ok\":false}");
        return true;
    }

    p_wifi->print("HTTP/1.1 200 OK\r\n");
    p_wifi->print("Content-Type: application/json\r\n");
    p_wifi->print("Connection: close\r\n\r\n");
    p_wifi->print("{\"ok\":true");
    p_wifi->print(",\"mmpc\":");
    p_wifi->print(getFloatLE(payload, 0), 5);
    p_wifi->print(",\"track\":");
    p_wifi->print(getFloatLE(payload, 4), 2);
    p_wifi->print(",\"kp\":");
    p_wifi->print(getFloatLE(payload, 8), 3);
    p_wifi->print(",\"ki\":");
    p_wifi->print(getFloatLE(payload, 12), 3);
    p_wifi->print(",\"kd\":");
    p_wifi->print(getFloatLE(payload, 16), 3);
    p_wifi->print("}");

    return true;
}

bool StreamServer::handleSetParams(void *p_client, const String &request_line)
{
    WiFiClient  *p_wifi = (WiFiClient *)p_client;
    LinkResponse type   = LinkResponse::NACK;
    uint8_t      resp[LINK_MAX_PAYLOAD] = {0};
    uint8_t      resp_len = 0;
    uint8_t      payload[20] = {0};
    bool         ok = false;

    putFloatLE(payload, 0,  queryFloat(request_line, "mmpc=", 0.0f));
    putFloatLE(payload, 4,  queryFloat(request_line, "track=", 0.0f));
    putFloatLE(payload, 8,  queryFloat(request_line, "kp=", 0.0f));
    putFloatLE(payload, 12, queryFloat(request_line, "ki=", 0.0f));
    putFloatLE(payload, 16, queryFloat(request_line, "kd=", 0.0f));

    if (m_pLink != nullptr)
    {
        ok = m_pLink->command(LinkCommand::SET_PARAMS, payload, 20, type, resp, resp_len, 500);
    }

    if ((ok == true) && (type == LinkResponse::ACK))
    {
        sendJson(p_wifi, "{\"ok\":true,\"msg\":\"params set\"}");
    }
    else
    {
        sendJson(p_wifi, "{\"ok\":false,\"msg\":\"no ack\"}");
    }

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

bool StreamServer::handleClients()
{
    WiFiClient client = gControlServer.accept();

    if (client)
    {
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

        String request_line = client.readStringUntil('\n');

        while (client.available() > 0)
        {
            String header = client.readStringUntil('\n');

            if (header.length() <= 1)
            {
                break;
            }
        }

        bool is_stream = false;

        if (request_line.indexOf("/stream") >= 0)
        {
            s_streamClient = client;
            writeStreamHeader(&s_streamClient);
            s_lastFrameMs = 0;
            is_stream = true;
        }
        else if (request_line.indexOf("/cmd/ping") >= 0)
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
        else if (request_line.indexOf("/cmd/getparams") >= 0)
        {
            handleGetParams(&client);
        }
        else if (request_line.indexOf("/cmd/setparams") >= 0)
        {
            handleSetParams(&client, request_line);
        }
        else if (request_line.indexOf("/cmd/drive") >= 0)
        {
            handleDrive(&client, request_line);
        }
        else
        {
            sendIndexPage(&client);
        }

        if (is_stream == false)
        {
            client.stop();
        }
    }

    pumpStream();

    return true;
}
