#pragma once

#define CAM_WIDTH                   320
#define CAM_HEIGHT                  240
#define CAM_FRAME_BYTES             (CAM_WIDTH * CAM_HEIGHT)

#define CAM_RAW_WIDTH               324
#define CAM_RAW_HEIGHT              244
#define CAM_RAW_LINE_BYTES          (CAM_RAW_WIDTH * 2)
#define CAM_RAW_FRAME_BYTES         (CAM_RAW_LINE_BYTES * CAM_RAW_HEIGHT)

#define CAM_NIBBLE_LSB_FIRST        1

#define CAM_DVP_CTLR_ID             0
#define CAM_DVP_DMA_BURST           64
#define CAM_DVP_H_RES               CAM_RAW_LINE_BYTES
#define CAM_DVP_V_RES               CAM_RAW_HEIGHT
#define CAM_RAW_ALLOC_ALIGN         64

// Capture buffer size. The real frame is 158112 bytes (confirmed by
// received_size); this keeps a margin above it without the driver having to
// cache-sync a needlessly large region every frame.
#define CAM_RAW_ALLOC_BYTES         163840

#define CAM_FRAME_TIMEOUT_MS        200

#define CAM_START_WITH_TEST_PATTERN 1

#define CAM_I2C_ADDR                0x24
#define CAM_I2C_HZ                  400000
#define CAM_JPEG_QUALITY            80

#define HM01B0_MODEL_ID             0x01B0

#define HM01B0_REG_MODEL_ID_H       0x0000
#define HM01B0_REG_MODEL_ID_L       0x0001
#define HM01B0_REG_MODE_SELECT      0x0100
#define HM01B0_REG_SW_RESET         0x0103
#define HM01B0_REG_GRP_PARAM_HOLD   0x0104
#define HM01B0_REG_TEST_PATTERN     0x0601
#define HM01B0_REG_QVGA_WIN_EN      0x3010
#define HM01B0_REG_BIT_CONTROL      0x3059
#define HM01B0_REG_OSC_CLK_DIV      0x3060

#define HM01B0_MODE_STANDBY         0x00
#define HM01B0_MODE_STREAMING       0x01

#define HM01B0_BITCTRL_8BIT         0x02
#define HM01B0_BITCTRL_4BIT         0x42
#define HM01B0_BITCTRL_1BIT         0x22

#define HM01B0_TP_OFF               0x00
#define HM01B0_TP_COLOR_BAR         0x01
#define HM01B0_TP_WALKING_ONES      0x11

#define HM01B0_GRP_HOLD             0x01
#define HM01B0_GRP_RELEASE          0x00