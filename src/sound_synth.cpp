#include "sound_synth.h"
#include "logger.h"
#include <math.h>
#include <esp_heap_caps.h>


// Preset table, ordered to match the Emote enum. Values ported from the fitted
// Python model. Contour is 8 evenly-spaced pitch multipliers.
static const SynthPreset s_presets[(int)Emote::COUNT] =
{
    // CURIOUS
    {
        2400.0f, 0.424f, 0.60f, 2.40f, 17.6f, 0.198f, 0.48f, 0.050f, 2530.0f,
        {1.00f, 1.20f, 1.29f, 1.62f, 1.61f, 1.63f, 1.58f, 1.58f}
    },
    // SURPRISED
    {
        714.0f, 0.485f, 0.82f, 1.62f, 5.9f, 0.203f, 0.45f, 0.004f, 3050.0f,
        {1.00f, 1.75f, 3.63f, 4.21f, 3.51f, 3.05f, 2.41f, 1.87f}
    },
    // ANGRY
    {
        2400.0f, 0.655f, 1.53f, 2.46f, 5.9f, 0.169f, 0.26f, 0.050f, 5600.0f,
        {1.00f, 0.69f, 1.30f, 1.51f, 1.16f, 0.60f, 0.53f, 0.53f}
    },
    // QUESTION
    {
        1060.0f, 0.506f, 1.15f, 0.85f, 5.9f, 0.021f, 0.06f, 0.004f, 1010.0f,
        {1.00f, 1.65f, 1.69f, 2.38f, 2.30f, 3.15f, 3.12f, 3.50f}
    },
    // REFUSE
    {
        503.0f, 0.356f, 3.11f, 0.97f, 5.9f, 0.045f, 0.14f, 0.004f, 3260.0f,
        {1.00f, 1.01f, 3.42f, 3.65f, 1.96f, 1.09f, 0.91f, 0.58f}
    },
    // REALIZATION
    {
        880.0f, 0.615f, 0.68f, 0.65f, 11.7f, 0.012f, 0.19f, 0.004f, 760.0f,
        {1.00f, 1.24f, 1.02f, 0.96f, 0.87f, 0.96f, 1.04f, 1.06f}
    }
};

// Base formant frequencies in Hz, scaled per-preset. From the measured voices.
static const float s_formantHz[SYNTH_FORMANT_COUNT] = {500.0f, 780.0f, 930.0f, 1450.0f};
static const float s_formantQ[SYNTH_FORMANT_COUNT]  = {4.5f, 4.5f, 5.5f, 6.0f};
static const float s_formantG[SYNTH_FORMANT_COUNT]  = {1.00f, 0.75f, 0.55f, 0.18f};

// Small deterministic PRNG so a given seed always yields the same utterance.
static uint32_t s_rngState = 1;

static void rngSeed(uint32_t seed)
{
    if (seed == 0)
    {
        seed = 1;
    }

    s_rngState = seed;
}

static uint32_t rngNext()
{
    s_rngState ^= s_rngState << 13;
    s_rngState ^= s_rngState >> 17;
    s_rngState ^= s_rngState << 5;
    return s_rngState;
}

static float rngUniform(float lo, float hi)
{
    float u = (float)(rngNext() & 0xFFFFFF) / (float)0xFFFFFF;
    return lo + (hi - lo) * u;
}

bool SoundSynth::initialize()
{
    gLogger.attempt("Sound synth init");

    for (int i = 0; i < 1024; i++)
    {
        m_sineTable[i] = sinf(2.0f * (float)M_PI * (float)i / 1024.0f);
    }

    // Render scratch lives in PSRAM. It is 64 KB and is only touched while an
    // utterance is being built, so it does not belong in internal SRAM where it
    // would sit reserved for the life of the process.
    m_pScratch = (float *)heap_caps_malloc(SYNTH_MAX_SAMPLES * sizeof(float), MALLOC_CAP_SPIRAM);

    if (m_pScratch == nullptr)
    {
        gLogger.failure("Sound synth scratch alloc");
        return false;
    }

    m_ready = true;
    gLogger.success("Sound synth init");
    return true;
}

float SoundSynth::sine(float phase_01)
{
    // phase_01 is the fractional phase in 0..1. Linear-interpolated table lookup.
    float x = phase_01 - floorf(phase_01);
    float fidx = x * 1024.0f;
    int i0 = (int)fidx;
    int i1 = (i0 + 1) & 1023;
    float frac = fidx - (float)i0;
    return m_sineTable[i0] + (m_sineTable[i1] - m_sineTable[i0]) * frac;
}

float SoundSynth::resonator(float in, float freq_hz, float q, float &m_z1, float &m_z2)
{
    // Two-pole resonator, evaluated one sample at a time so the same state runs
    // across the whole utterance. Coefficients recomputed per call are cheap
    // relative to the sine/FM work and keep the formant fixed as pitch glides.
    float w = 2.0f * (float)M_PI * freq_hz / (float)SYNTH_SAMPLE_RATE;
    float r = expf(-w / (2.0f * q));
    float a1 = -2.0f * r * cosf(w);
    float a2 = r * r;

    // Peak-gain normalization: without the sqrt term the low formants get
    // several times too much gain, which over-brightens presets that lean on
    // them (realization, question). This matches the Python resonator exactly.
    float b0 = (1.0f - r) * sqrtf(1.0f - 2.0f * r * cosf(2.0f * w) + r * r);

    float out = b0 * in - a1 * m_z1 - a2 * m_z2;
    m_z2 = m_z1;
    m_z1 = out;
    return out;
}

float SoundSynth::contourAt(const SynthPreset &preset, float t_01)
{
    // 8 breakpoints evenly spaced 0..1, cosine-interpolated between them.
    if (t_01 <= 0.0f)
    {
        return preset.contour[0];
    }

    if (t_01 >= 1.0f)
    {
        return preset.contour[SYNTH_CONTOUR_POINTS - 1];
    }

    float span = 1.0f / (float)(SYNTH_CONTOUR_POINTS - 1);
    int i = (int)(t_01 / span);

    if (i >= SYNTH_CONTOUR_POINTS - 1)
    {
        i = SYNTH_CONTOUR_POINTS - 2;
    }

    float frac = (t_01 - (float)i * span) / span;
    float smooth = 0.5f - 0.5f * cosf((float)M_PI * frac);
    return preset.contour[i] + (preset.contour[i + 1] - preset.contour[i]) * smooth;
}

bool SoundSynth::render(Emote emote, uint32_t seed, int16_t *p_out, size_t out_cap, size_t &out_len)
{
    if (m_ready == false)
    {
        return false;
    }

    if (p_out == nullptr)
    {
        gLogger.error("Sound synth render null buffer FAILED");
        return false;
    }

    if ((int)emote >= (int)Emote::COUNT)
    {
        gLogger.error("Sound synth render bad emote FAILED");
        return false;
    }

    const SynthPreset &preset = s_presets[(int)emote];
    rngSeed(seed);

    // Per-utterance randomization, matching the Python: small pitch transpose,
    // duration jitter, and an FM index wobble. This is what stops repeated
    // playback from sounding identical.
    float transpose = powf(2.0f, rngUniform(-1.0f, 1.0f) / 12.0f);
    float dur = preset.duration_s * rngUniform(0.88f, 1.12f);
    float fm_index = preset.fm_index * rngUniform(0.88f, 1.12f);
    float vib_hz = rngUniform(5.0f, 8.0f);
    float base_f0 = preset.base_f0 * transpose;

    size_t n = (size_t)(dur * (float)SYNTH_SAMPLE_RATE);

    if (n > out_cap)
    {
        n = out_cap;
    }

    if (n > SYNTH_MAX_SAMPLES)
    {
        n = SYNTH_MAX_SAMPLES;
    }

    // Envelope peak time, clamped so it never precedes the attack.
    float attack = preset.attack_s * dur / preset.duration_s;
    float peak = preset.peak_at * dur;

    if (peak < attack)
    {
        peak = attack;
    }

    if (peak > dur * 0.95f)
    {
        peak = dur * 0.95f;
    }

    float carrier_phase = 0.0f;
    float trem_phase0 = rngUniform(0.0f, 1.0f);

    float z1[SYNTH_FORMANT_COUNT] = {0};
    float z2[SYNTH_FORMANT_COUNT] = {0};

    // Output bandpass as one biquad, corners at 200 Hz and preset.output_hi_hz.
    // A biquad has a defined, predictable response as output_hi_hz is tuned by
    // ear, unlike the gentle one-pole it replaces. Coefficients are computed
    // once per render (RBJ bandpass, constant skirt gain).
    float bp_center = sqrtf(200.0f * preset.output_hi_hz);
    float bp_bw = preset.output_hi_hz - 200.0f;

    if (bp_bw < 100.0f)
    {
        bp_bw = 100.0f;
    }

    float bp_w0 = 2.0f * (float)M_PI * bp_center / (float)SYNTH_SAMPLE_RATE;
    float bp_q = bp_center / bp_bw;
    float bp_alpha = sinf(bp_w0) / (2.0f * bp_q);
    float bp_a0 = 1.0f + bp_alpha;
    float bp_b0 = bp_alpha / bp_a0;
    float bp_b2 = -bp_alpha / bp_a0;
    float bp_a1 = (-2.0f * cosf(bp_w0)) / bp_a0;
    float bp_a2 = (1.0f - bp_alpha) / bp_a0;
    float bp_x1 = 0.0f;
    float bp_x2 = 0.0f;
    float bp_y1 = 0.0f;
    float bp_y2 = 0.0f;

    // Slow random-walk pitch jitter, one-pole smoothed, in cents.
    float jitter = 0.0f;

    float peak_abs = 0.0001f;
    float *p_scratch = m_pScratch;

    for (size_t i = 0; i < n; i++)
    {
        float t = (float)i / (float)SYNTH_SAMPLE_RATE;
        float t01 = t / dur;

        jitter = 0.995f * jitter + 0.005f * rngUniform(-1.0f, 1.0f);
        float vib = sinf(2.0f * (float)M_PI * vib_hz * t);
        float cents = 18.0f * jitter + 25.0f * vib;
        float f0 = base_f0 * contourAt(preset, t01) * powf(2.0f, cents / 1200.0f);

        carrier_phase += f0 / (float)SYNTH_SAMPLE_RATE;
        float mod = fm_index * sine(SYNTH_FM_RATIO * carrier_phase);
        float w = sine(carrier_phase + mod);

        // A little second and third harmonic, tapered as fm_index rises, to
        // match the Python. Keeps bright presets from overshooting bandwidth.
        float h2 = 0.30f * expf(-fm_index / 2.5f);
        float h3 = 0.10f * expf(-fm_index / 2.0f);
        w = (w + h2 * sine(2.0f * carrier_phase + mod) + h3 * sine(3.0f * carrier_phase + mod))
            / (1.0f + h2 + h3);

        if (preset.breath > 0.0f)
        {
            w += preset.breath * rngUniform(-1.0f, 1.0f);
        }

        // Soft clip.
        w = tanhf(w * 1.5f);

        // Formant bank, scaled per-preset.
        float dry = 0.30f;
        float body = dry * w;
        float gsum = dry;

        for (int k = 0; k < SYNTH_FORMANT_COUNT; k++)
        {
            float ff = s_formantHz[k] * preset.formant_scale;

            if (ff > (float)SYNTH_SAMPLE_RATE * 0.45f)
            {
                ff = (float)SYNTH_SAMPLE_RATE * 0.45f;
            }

            body += s_formantG[k] * resonator(w, ff, s_formantQ[k], z1[k], z2[k]);
            gsum += s_formantG[k];
        }

        body = body / gsum;

        // Envelope: rise to peak, exponential decay after.
        float env = 0.0f;

        if (t < peak)
        {
            env = t / attack;

            if (env > 1.0f)
            {
                env = 1.0f;
            }
        }
        else
        {
            float tau = (dur - peak) / 2.2f;

            if (tau < 0.001f)
            {
                tau = 0.001f;
            }

            env = expf(-(t - peak) / tau);
        }

        // Tremolo.
        float trem = 1.0f - 0.50f * 0.5f * (1.0f - cosf(2.0f * (float)M_PI * (preset.trem_hz * t + trem_phase0)));
        env = env * trem;

        float sample = body * env;

        // Biquad bandpass.
        float bp_out = bp_b0 * sample + bp_b2 * bp_x2 - bp_a1 * bp_y1 - bp_a2 * bp_y2;
        bp_x2 = bp_x1;
        bp_x1 = sample;
        bp_y2 = bp_y1;
        bp_y1 = bp_out;
        sample = bp_out;

        p_scratch[i] = sample;

        float a = fabsf(sample);

        if (a > peak_abs)
        {
            peak_abs = a;
        }
    }

    // Short release cosine on the tail to avoid a click at the end.
    size_t r = (size_t)(0.018f * (float)SYNTH_SAMPLE_RATE);

    if (r > n)
    {
        r = n;
    }

    for (size_t i = 0; i < r; i++)
    {
        float k = 0.5f + 0.5f * cosf((float)M_PI * (float)i / (float)r);
        p_scratch[n - r + i] *= k;
    }

    // Normalize to a fixed headroom and convert to int16.
    float norm = 0.7f / peak_abs;

    for (size_t i = 0; i < n; i++)
    {
        float v = p_scratch[i] * norm;

        if (v > 1.0f)
        {
            v = 1.0f;
        }

        if (v < -1.0f)
        {
            v = -1.0f;
        }

        p_out[i] = (int16_t)(v * 32767.0f);
    }

    out_len = n;
    return true;
}
