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

// HM01B0 runs in 4-bit output mode: the breakout only wires D0-D3, and each
// 8-bit pixel is serialized as two low-nibble bytes over D0-D3. See
// camera_globals.h and the capture loop in camera.cpp.
//
// D4-D7 are mirrored onto D0 rather than given their own GPIOs. The reasons:
//
//   1. The upper four lines are unwired, so their value is meaningless. The
//      capture loop masks every byte with & 0x0F before use, which discards
//      them unconditionally.
//   2. ll_cam_set_pin has no guard for an unused data pin. It indexes
//      GPIO_PIN_MUX_REG[pin] for all eight, so a negative pin is a wild array
//      read and faults at camera init.
//   3. The GPIO matrix allows one pin to drive many peripheral input signals,
//      so routing GPIO 17 to five camera data inputs is legal and costs
//      nothing. The driver configures it as a floating input five times, which
//      is idempotent.
//
// The point of this is to keep the camera off GPIO 15/16/40/41. Those four
// belong to the audio subsystem: the shared I2S BCLK/WS and the amp/mic data
// lines. The previous 8-bit defines put the camera and the amp on the same
// pins, which only conflicts once camera and audio run in the same firmware.
//
// Do not give D4-D7 real GPIO numbers. Every usable pin on this board is
// already assigned, and any pin named here is claimed as a camera input for
// the life of the process.
#define PIN_CAM_D0       17
#define PIN_CAM_D1       18
#define PIN_CAM_D2       38
#define PIN_CAM_D3       39
#define PIN_CAM_D4       PIN_CAM_D0
#define PIN_CAM_D5       PIN_CAM_D0
#define PIN_CAM_D6       PIN_CAM_D0
#define PIN_CAM_D7       PIN_CAM_D0
#define PIN_CAM_XCLK     42

#define PIN_STATUS_LED   21

// Audio subsystem. One I2S controller in full duplex: BCLK and WS are shared
// between the INMP441 mic and the MAX98357A amp, so only the two data pins and
// the amp SD_MODE are separate. See sound_globals.h for format and the ESP32
// hardware Notion doc for the wiring rationale.
#define PIN_I2S_BCLK     15
#define PIN_I2S_WS       16
#define PIN_I2S_MIC_SD   40
#define PIN_I2S_AMP_DIN  41
#define PIN_AMP_SD_MODE  14
