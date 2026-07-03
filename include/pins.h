#pragma once

#define PIN_TFT_SCK      1
#define PIN_TFT_MOSI     2
#define PIN_TFT_CS       3
#define PIN_TFT_DC       4
#define PIN_TFT_RST      5
#define PIN_TFT_BL       6

#define PIN_LINK_TX      7
#define PIN_LINK_RX      13

#define PIN_CAM_SDA      8
#define PIN_CAM_SCL      9
#define PIN_CAM_PCLK     10
#define PIN_CAM_VSYNC    11
#define PIN_CAM_HREF     12

// HM01B0 runs as an 8-bit DVP sensor on the esp32-camera path, so all of D0-D7
// are wired. D0 is the bus LSB. XCLK is now driven by the ESP (LEDC), so the
// breakout must be switched to accept an external clock (lift the onboard clock
// resistor) rather than self-oscillating.
//
// CONFIRM each of these against the S3-Zero pinout before wiring. Hard
// constraints: avoid 14 (link RX, above), 0/3/45/46 (strapping), 19/20 (USB),
// and 26-32 (flash/PSRAM). D2/D3 already sit on the back pads (38/39); D4-D7 and
// XCLK below are their neighbours, but verify they are actually broken out.
#define PIN_CAM_D0       17
#define PIN_CAM_D1       18
#define PIN_CAM_D2       38
#define PIN_CAM_D3       39
#define PIN_CAM_D4       15
#define PIN_CAM_D5       16
#define PIN_CAM_D6       40
#define PIN_CAM_D7       41
#define PIN_CAM_XCLK     42

#define PIN_STATUS_LED   21