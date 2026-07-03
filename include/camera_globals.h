#pragma once

#define CAM_WIDTH                   320
#define CAM_HEIGHT                  232
#define CAM_FRAME_BYTES             (CAM_WIDTH * CAM_HEIGHT)

// The breakout only wires D0-D3, so the sensor runs in 4-bit output mode: it
// serializes each 8-bit pixel as two low-nibble bytes over D0-D3. D4-D7 carry
// nothing and are masked off in reassembly. The measured raw line stride is 652
// bytes (= 326 pixel pairs, 2 x the 326 seen in 8-bit). cam_hal grabs a
// 672x236 = 158592 byte frame (that config keeps FB-SIZE happy), and we
// reassemble at the real 652 stride inside it, cropping the 6-pixel border.
#define CAM_RAW_LINE_BYTES          652
#define CAM_RAW_LINES               234
#define CAM_CROP_X                  3    // pixels trimmed from the left (326 -> 320, centred)
#define CAM_CROP_Y                  2    // rows trimmed from the top

// LSB-first: the first byte of each pair holds the low nibble. Flip to 0 if the
// reassembled image looks inverted or scrambled in tone.
#define CAM_NIBBLE_LSB_FIRST        1

// XCLK from the ESP via LEDC, divided from 80 MHz (20, 16, 10, 8). Lower this
// first if the capture layer logs FB-OVF.
#define CAM_XCLK_FREQ_HZ            20000000

#define CAM_FB_COUNT                2
#define CAM_JPEG_QUALITY            80
#define CAM_START_WITH_TEST_PATTERN 0