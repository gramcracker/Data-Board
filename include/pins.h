#pragma once

#define PIN_TFT_SCK      1
#define PIN_TFT_MOSI     2
#define PIN_TFT_CS       3
#define PIN_TFT_DC       4
#define PIN_TFT_RST      5
#define PIN_TFT_BL       6

#define PIN_LINK_TX      7
#define PIN_LINK_RX      14

#define PIN_CAM_SDA      8
#define PIN_CAM_SCL      9
#define PIN_CAM_PCLK     10
#define PIN_CAM_VSYNC    11
#define PIN_CAM_HREF     12

// HM01B0 4-bit data bus wired to camera D0-D3. The S3 LCD_CAM peripheral has no
// native 4-bit capture mode, so it runs as 8-bit and the upper four data inputs
// (D4-D7) are routed to constant low via GPIO_NUM_NC in startCapture, costing no
// physical pins. These are on the board back-side pads. Avoid strapping pin 45
// and the USB pair (19, 20). Wire breakout D0 to PIN_CAM_D0 (the bus LSB), D1 to
// PIN_CAM_D1, and so on, so the captured byte bit order matches the reassembly.
#define PIN_CAM_D0       17
#define PIN_CAM_D1       18
#define PIN_CAM_D2       38
#define PIN_CAM_D3       39

#define PIN_STATUS_LED   21