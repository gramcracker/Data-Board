#pragma once

#include <stdint.h>

// Synthesis constants ported from the fitted Python model (mochan_voice.py).
// Each emote's contour, duration, attack, tremolo rate, FM index, and formant
// scale were solved to match measured reference clips. See the Audio Notion doc
// for the derivation. These are the character of the voice; treat them as data.

#define SYNTH_SAMPLE_RATE           16000
#define SYNTH_MAX_SAMPLES           16000       // 1.0 s ceiling per utterance at 16 kHz

// The voice's timbre comes from an irrational carrier-to-modulator FM ratio.
// Integer ratios sound like a clean beep; 1.41 gives the inharmonic grain that
// reads as a robot voice.
#define SYNTH_FM_RATIO              1.41f

// Fixed formant resonators, in Hz, scaled per-emote by formant_scale. Fixed
// formants that do not move as the pitch glides are the vocal-tract cue that
// separates a voice from a beep.
#define SYNTH_FORMANT_COUNT         4

// Contour is 8 breakpoints, evenly spaced in time from 0.0 to 1.0. Each value
// is a pitch multiplier applied to base_f0.
#define SYNTH_CONTOUR_POINTS        8

enum class Emote : uint8_t
{
    CURIOUS     = 0,
    SURPRISED   = 1,
    ANGRY       = 2,
    QUESTION    = 3,
    REFUSE      = 4,
    REALIZATION = 5,
    COUNT       = 6
};

struct SynthPreset
{
    float    base_f0;
    float    duration_s;
    float    fm_index;
    float    formant_scale;
    float    trem_hz;
    float    attack_s;
    float    peak_at;
    float    breath;
    float    output_hi_hz;
    float    contour[SYNTH_CONTOUR_POINTS];
};
