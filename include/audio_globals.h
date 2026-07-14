#pragma once

#include <stdint.h>

// Pin map is the canonical Data-board assignment. Audio shares one I2S
// controller in full duplex: BCLK and WS are common to the mic and the amp,
// so only the two data pins and the amp SD_MODE are separate.

#define PIN_I2S_BCLK                15
#define PIN_I2S_WS                  16
#define PIN_I2S_MIC_SD              40
#define PIN_I2S_AMP_DIN             41
#define PIN_AMP_SD_MODE             14

// The shared clocks force one sample rate and slot width for both devices.
// 16 kHz, 32-bit slots, stereo, giving BCLK 1.024 MHz. The tone test only
// drives the amp, but it uses the same clock config the full duplex path will,
// so a clean tone here validates the clocking the mic will later depend on.

#define AUDIO_SAMPLE_RATE           16000
#define AUDIO_SLOT_BITS             32
#define AUDIO_CHANNELS              2

// The MAX98357A occupies the left slot: the INMP441 L/R pin is tied to GND
// which puts the mic in the left slot, and SD_MODE driven high selects left
// for the amp, so both agree on the slot. Audio samples are int16 placed in
// the top of the 32-bit slot; the low 16 bits are zero.

#define AUDIO_TONE_HZ               800
#define AUDIO_TONE_AMPLITUDE        8000        // out of 32767, deliberately low for the first test

// SD_MODE trip points from the datasheet, for reference:
//   below 0.16 V   shutdown
//   0.16 to 0.77 V (L+R)/2
//   0.77 to 1.4 V  right
//   above 1.4 V    left
// A 3V3 GPIO high selects left; low is shutdown and doubles as the hard mute.

#define AMP_SD_MODE_ENABLE          1
#define AMP_SD_MODE_SHUTDOWN        0
