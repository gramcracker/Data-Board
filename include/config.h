#pragma once

#define ENABLE_SCREEN      1
#define ENABLE_CAMERA      1
#define ENABLE_WIFI        1
#define ENABLE_LINK        1
#define ENABLE_STREAM      1

// BLE provisioning. The app shows PROV_SERVICE_NAME as the device to connect
// to, and the user enters PROV_POP as the pairing pin. Set PROV_FORCE_RESET to
// 1 to wipe stored credentials and force re-provisioning (development only).
#define PROV_SERVICE_NAME  "PROV_MOCHAN"
#define PROV_POP           "mochan1234"
#define PROV_FORCE_RESET   0

#define LINK_BAUD          115200
#define STREAM_PORT        80
