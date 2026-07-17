#pragma once

#include <stdint.h>
#include <stddef.h>
#include "sound_globals.h"

class SoundSynth
{
public:
    bool initialize();

    // Renders one emote into p_out as int16 mono at SYNTH_SAMPLE_RATE. out_len
    // receives the sample count. seed makes the per-utterance randomization
    // reproducible, which the host logs so the episodic log can reconstruct
    // what the robot sounded like.
    bool render(Emote emote, uint32_t seed, int16_t *p_out, size_t out_cap, size_t &out_len);

private:
    float  m_sineTable[1024] = {0};
    float *m_pScratch = nullptr;
    bool   m_ready = false;

    float sine(float phase_01);
    float resonator(float in, float freq_hz, float q, float &m_z1, float &m_z2);
    float contourAt(const SynthPreset &preset, float t_01);
};

