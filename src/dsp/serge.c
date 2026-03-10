/**
 * Serge — Move Anything sound generator
 * Author: Vincent Fillion
 * License: MIT
 * Architecture: Monophonic, West Coast synthesis
 *
 * Inspired by Serge Modular La Bestia III:
 *   Complex Oscillator | NTO | VCFQ+ Filter | GTO+ | Wave Multiplier
 *
 * API: plugin_api_v2_t
 * Audio: 44100Hz, 128 frames/block, stereo interleaved int16 output
 *
 * Parameter categories (jog-wheel navigation):
 *   1. Oscillators — OSC1 Pitch, OSC1 Timbre, OSC2 Pitch, OSC2 Harmonics, OSC Mix
 *   2. Wavefold   — Fold Depth, Fold Type
 *   3. Filter     — Cutoff, Q
 *   4. Envelope   — Attack, Decay, Sustain, Release
 *   5. Modulation — GTO Rate, LFO Waveform, Portamento, Legato
 *
 * Modulation matrix: 4 rows (sources) x 8 columns (targets) = 32 pads
 *   Row 0: Dual Oscillators (audio-rate feedback)
 *   Row 1: GTO Smooth (triangle LFO)
 *   Row 2: GTO Stepped (S&H random)
 *   Row 3: Wave Multiplier (folded audio)
 *   Col 0-7: OSC1 Pitch, OSC1 Timbre, OSC2 Pitch, OSC2 Harmonics,
 *            Fold Depth, Fold Type, Filter Cutoff, Output Level
 */

#include <stdlib.h>
#include <string.h>
#include <math.h>
#include <stdint.h>

#define SAMPLE_RATE   44100.0f
#define FRAMES_PER_BLOCK 128
#define PI            3.14159265359f
#define TWO_PI        6.28318530718f
#define DENORMAL_GUARD 1e-20f

/* ── Modulation matrix dimensions ────────────────────────────────────────────── */

#define MOD_ROWS 4
#define MOD_COLS 8

/* Source rows */
#define SRC_DUAL_OSC     0
#define SRC_GTO_SMOOTH   1
#define SRC_GTO_STEPPED  2
#define SRC_WAVE_MULT    3

/* Target columns */
#define TGT_OSC1_PITCH    0
#define TGT_OSC1_TIMBRE   1
#define TGT_OSC2_PITCH    2
#define TGT_OSC2_HARMONICS 3
#define TGT_FOLD_DEPTH    4
#define TGT_FOLD_TYPE     5
#define TGT_FILTER_CUTOFF 6
#define TGT_OUTPUT_LEVEL  7

/* Move pad MIDI note mapping: pads 68-99, 4 rows x 8 cols, bottom-left to top-right */
#define PAD_NOTE_BASE 68

/* ── Complex Oscillator (waveform morphing: sine->tri->saw->pulse) ───────── */

typedef struct {
    float phase;
    float freq;
} complex_osc_t;

static float complex_osc_sample(complex_osc_t *osc, float freq_hz, float timbre) {
    osc->freq = freq_hz;
    osc->phase += freq_hz / SAMPLE_RATE;
    if (osc->phase >= 1.0f) osc->phase -= 1.0f;
    if (osc->phase < 0.0f) osc->phase += 1.0f;

    float p = osc->phase;
    float sine = sinf(p * TWO_PI);

    /* Triangle */
    float tri;
    if (p < 0.25f)      tri = p * 4.0f;
    else if (p < 0.75f) tri = 2.0f - p * 4.0f;
    else                tri = p * 4.0f - 4.0f;

    /* Sawtooth */
    float saw = 2.0f * p - 1.0f;

    /* Pulse (50% duty) */
    float pulse = (p < 0.5f) ? 1.0f : -1.0f;

    /* Morph: 0=sine, 0.33=tri, 0.67=saw, 1.0=pulse */
    float out;
    if (timbre < 0.33f) {
        float blend = timbre / 0.33f;
        out = sine * (1.0f - blend) + tri * blend;
    } else if (timbre < 0.67f) {
        float blend = (timbre - 0.33f) / 0.34f;
        out = tri * (1.0f - blend) + saw * blend;
    } else {
        float blend = (timbre - 0.67f) / 0.33f;
        out = saw * (1.0f - blend) + pulse * blend;
    }
    return out * 0.8f;
}

/* ── NTO (New Timbral Oscillator — harmonic selection) ───────────────────── */

typedef struct {
    float phase;
    float freq;
} nto_t;

static float nto_sample(nto_t *nto, float freq_hz, float harmonics) {
    nto->freq = freq_hz;
    nto->phase += freq_hz / SAMPLE_RATE;
    if (nto->phase >= 1.0f) nto->phase -= 1.0f;
    if (nto->phase < 0.0f) nto->phase += 1.0f;

    float pa = nto->phase * TWO_PI;
    float out;

    if (harmonics < 0.33f) {
        /* Sine-like: fundamental + subtle odd harmonics */
        float blend = harmonics / 0.33f;
        out = sinf(pa)
            + blend * 0.3f * sinf(3.0f * pa) / 3.0f
            + blend * 0.15f * sinf(5.0f * pa) / 5.0f;
    } else if (harmonics < 0.67f) {
        /* Full series: both odd and even */
        float blend = (harmonics - 0.33f) / 0.34f;
        float base = sinf(pa)
            + 0.3f * sinf(3.0f * pa) / 3.0f
            + 0.15f * sinf(5.0f * pa) / 5.0f;
        float even = 0.25f * sinf(2.0f * pa) / 2.0f
            + 0.15f * sinf(4.0f * pa) / 4.0f
            + 0.1f * sinf(6.0f * pa) / 6.0f;
        out = base + blend * even;
    } else {
        /* Bell-like: even harmonics emphasized */
        float blend = (harmonics - 0.67f) / 0.33f;
        out = sinf(pa)
            + 0.3f * sinf(2.0f * pa) / 2.0f
            + 0.2f * sinf(4.0f * pa) / 4.0f
            + 0.15f * sinf(6.0f * pa) / 6.0f
            + blend * 0.1f * sinf(8.0f * pa) / 8.0f;
    }
    return out * 0.8f;
}

/* ── GTO+ Dual Slope Generator ───────────────────────────────────────────── */

typedef struct {
    float smooth_phase;
    float stepped_value;
    uint32_t step_counter;
    uint32_t rng_state;
} gto_plus_t;

static float gto_random(gto_plus_t *gto) {
    gto->rng_state = (gto->rng_state * 1103515245u + 12345u) & 0x7fffffffu;
    return 2.0f * ((float)gto->rng_state / (float)0x7fffffff) - 1.0f;
}

static void gto_tick(gto_plus_t *gto, float rate_hz,
                     float *smooth_out, float *stepped_out) {
    /* Smooth side: triangle LFO */
    gto->smooth_phase += rate_hz / SAMPLE_RATE;
    if (gto->smooth_phase >= 1.0f) gto->smooth_phase -= 1.0f;

    float p = gto->smooth_phase;
    float tri;
    if (p < 0.25f)      tri = p * 4.0f;
    else if (p < 0.75f) tri = 2.0f - p * 4.0f;
    else                tri = p * 4.0f - 4.0f;
    /* Bipolar -1 to +1 */
    *smooth_out = tri;

    /* Stepped side: S&H at same rate */
    uint32_t period = (uint32_t)(SAMPLE_RATE / fmaxf(rate_hz, 0.01f));
    gto->step_counter++;
    if (gto->step_counter >= period) {
        gto->stepped_value = gto_random(gto);
        gto->step_counter = 0;
    }
    *stepped_out = gto->stepped_value;
}

/* ── Wave Multiplier (harmonic wavefolding) ──────────────────────────────── */

static float wavefold(float input, float depth, float fold_type) {
    if (depth < 0.001f) return input;

    float driven = input * (1.0f + depth * 8.0f);

    /* Fold reflections (up to 3 stages) */
    for (int j = 0; j < 3; j++) {
        if (driven > 1.0f)       driven = 2.0f - driven;
        else if (driven < -1.0f) driven = -2.0f - driven;
        else break;
    }

    /* Fold type: 0=odd (sine-shaped), 0.5=both (pass-through), 1.0=even (rectified) */
    float folded;
    if (fold_type < 0.5f) {
        float blend = fold_type / 0.5f;
        float odd = driven * sinf(input * PI);
        folded = odd * (1.0f - blend) + driven * blend;
    } else {
        float blend = (fold_type - 0.5f) / 0.5f;
        float even = fabsf(driven);
        folded = driven * (1.0f - blend) + even * blend;
    }

    /* Blend with dry based on depth */
    return input * (1.0f - depth * 0.6f) + folded * depth * 0.6f;
}

/* ── VCFQ+ (4-pole ladder filter with self-oscillation) ──────────────────── */

typedef struct {
    float stage[4];
    float feedback;
} vcfq_plus_t;

static float fast_tanh(float x) {
    if (x > 2.0f) return 1.0f;
    if (x < -2.0f) return -1.0f;
    float x2 = x * x;
    return x * (27.0f + x2) / (27.0f + 9.0f * x2);
}

static float vcfq_process(vcfq_plus_t *f, float input, float cutoff_hz, float q) {
    cutoff_hz = fmaxf(20.0f, fminf(20000.0f, cutoff_hz));
    q = fmaxf(0.0f, fminf(1.0f, q));

    float g = tanf(PI * cutoff_hz / SAMPLE_RATE);
    float resonance = q * q * 4.0f;

    float in_fb = input - resonance * f->feedback;

    /* 4-pole cascade */
    for (int i = 0; i < 4; i++) {
        float v = (in_fb - f->stage[i]) * g / (1.0f + g);
        f->stage[i] += 2.0f * v + DENORMAL_GUARD;
        in_fb = f->stage[i];
    }

    float out = fast_tanh(f->stage[3] * 0.7f);
    f->feedback = out;
    return out;
}

/* ── ADSR Envelope ───────────────────────────────────────────────────────── */

typedef struct {
    float level;
    int   stage;  /* 0=attack 1=decay 2=sustain 3=release 4=off */
} adsr_t;

static float adsr_tick(adsr_t *env, float a, float d, float s, float r) {
    float rate;
    switch (env->stage) {
        case 0: /* attack */
            rate = 1.0f / fmaxf(a * SAMPLE_RATE, 1.0f);
            env->level += rate;
            if (env->level >= 1.0f) { env->level = 1.0f; env->stage = 1; }
            break;
        case 1: /* decay */
            rate = (1.0f - s) / fmaxf(d * SAMPLE_RATE, 1.0f);
            env->level -= rate;
            if (env->level <= s) { env->level = s; env->stage = 2; }
            break;
        case 2: /* sustain */
            env->level = s;
            break;
        case 3: /* release */
            rate = env->level / fmaxf(r * SAMPLE_RATE, 1.0f);
            env->level -= rate;
            if (env->level <= 0.001f) { env->level = 0.0f; env->stage = 4; }
            break;
        default:
            env->level = 0.0f;
            break;
    }
    return env->level;
}

/* ── Synth instance ──────────────────────────────────────────────────────── */

typedef struct {
    /* DSP modules */
    complex_osc_t osc1;
    nto_t         osc2;
    gto_plus_t    gto;
    vcfq_plus_t   filter;
    adsr_t        env;

    /* Voice state */
    int   note_active;
    int   midi_note;
    float velocity;
    float freq_target;   /* from encoder pitch, not MIDI note */
    float freq_current;  /* with portamento */

    /* Modulation matrix: [row][col] = depth 0.0-1.0 */
    float mod_matrix[MOD_ROWS][MOD_COLS];

    /* Last modulation source outputs (for matrix computation) */
    float mod_src[MOD_ROWS];

    /* Encoder parameters (0-1 normalized unless noted) */
    float enc_osc1_pitch;     /* -2 to +5 V */
    float enc_osc1_timbre;    /* 0-1 */
    float enc_osc2_pitch;     /* -2 to +5 V */
    float enc_osc2_harmonics; /* 0-1 */
    float enc_fold_depth;     /* 0-1 */
    float enc_fold_type;      /* 0-1 */
    float enc_filter_cutoff;  /* 0-1 (mapped to Hz in render) */
    float enc_filter_q;       /* 0-1 */

    /* Menu parameters */
    float gto_rate;       /* Hz */
    float attack;         /* seconds */
    float decay;          /* seconds */
    float sustain;        /* 0-1 */
    float release;        /* seconds */
    float osc_mix;        /* 0-1 (0=OSC1 only, 1=OSC2 only, 0.5=equal) */
    float portamento;     /* seconds */
    int   legato;         /* 0 or 1 */
    int   lfo_waveform;   /* 0=tri, 1=sine, 2=saw (TODO: implement sine/saw) */

    /* Pitch bend / mod wheel */
    float pitch_bend;     /* semitones */
    float mod_wheel;      /* 0-1 */
} serge_instance_t;

/* ── CV voltage to frequency ─────────────────────────────────────────────── */

static float cv_to_freq(float cv) {
    /* 1V/octave, 0V = C3 (261.63 Hz) */
    return 261.63f * powf(2.0f, cv);
}

/* ── Compute modulated parameter values ──────────────────────────────────── */

typedef struct {
    float osc1_pitch_cv;
    float osc1_timbre;
    float osc2_pitch_cv;
    float osc2_harmonics;
    float fold_depth;
    float fold_type;
    float filter_cutoff_hz;
    float output_level;
} modulated_params_t;

static void compute_modulated_params(serge_instance_t *inst, modulated_params_t *p) {
    float *src = inst->mod_src;

    /* Col 0: OSC1 Pitch — mod range ±2V */
    p->osc1_pitch_cv = inst->enc_osc1_pitch;
    for (int r = 0; r < MOD_ROWS; r++)
        p->osc1_pitch_cv += src[r] * inst->mod_matrix[r][TGT_OSC1_PITCH] * 2.0f;
    p->osc1_pitch_cv = fmaxf(-2.0f, fminf(5.0f, p->osc1_pitch_cv));

    /* Col 1: OSC1 Timbre — mod range ±0.3 */
    p->osc1_timbre = inst->enc_osc1_timbre;
    for (int r = 0; r < MOD_ROWS; r++)
        p->osc1_timbre += src[r] * inst->mod_matrix[r][TGT_OSC1_TIMBRE] * 0.3f;
    p->osc1_timbre = fmaxf(0.0f, fminf(1.0f, p->osc1_timbre));

    /* Col 2: OSC2 Pitch — mod range ±2V */
    p->osc2_pitch_cv = inst->enc_osc2_pitch;
    for (int r = 0; r < MOD_ROWS; r++)
        p->osc2_pitch_cv += src[r] * inst->mod_matrix[r][TGT_OSC2_PITCH] * 2.0f;
    p->osc2_pitch_cv = fmaxf(-2.0f, fminf(5.0f, p->osc2_pitch_cv));

    /* Col 3: OSC2 Harmonics — mod range ±0.3 */
    p->osc2_harmonics = inst->enc_osc2_harmonics;
    for (int r = 0; r < MOD_ROWS; r++)
        p->osc2_harmonics += src[r] * inst->mod_matrix[r][TGT_OSC2_HARMONICS] * 0.3f;
    p->osc2_harmonics = fmaxf(0.0f, fminf(1.0f, p->osc2_harmonics));

    /* Col 4: Fold Depth — mod range ±0.3 */
    p->fold_depth = inst->enc_fold_depth;
    for (int r = 0; r < MOD_ROWS; r++)
        p->fold_depth += src[r] * inst->mod_matrix[r][TGT_FOLD_DEPTH] * 0.3f;
    p->fold_depth = fmaxf(0.0f, fminf(1.0f, p->fold_depth));

    /* Col 5: Fold Type — mod range ±0.3 */
    p->fold_type = inst->enc_fold_type;
    for (int r = 0; r < MOD_ROWS; r++)
        p->fold_type += src[r] * inst->mod_matrix[r][TGT_FOLD_TYPE] * 0.3f;
    p->fold_type = fmaxf(0.0f, fminf(1.0f, p->fold_type));

    /* Col 6: Filter Cutoff — logarithmic, mod range ±5kHz */
    float base_hz = 20.0f * powf(1000.0f, inst->enc_filter_cutoff);
    for (int r = 0; r < MOD_ROWS; r++)
        base_hz += src[r] * inst->mod_matrix[r][TGT_FILTER_CUTOFF] * 5000.0f;
    p->filter_cutoff_hz = fmaxf(20.0f, fminf(20000.0f, base_hz));

    /* Col 7: Output Level — base 1.0, mod range ±0.5 */
    p->output_level = 1.0f;
    for (int r = 0; r < MOD_ROWS; r++)
        p->output_level += src[r] * inst->mod_matrix[r][TGT_OUTPUT_LEVEL] * 0.5f;
    p->output_level = fmaxf(0.0f, fminf(1.5f, p->output_level));
}

/* ── Pad MIDI note to matrix [row, col] ──────────────────────────────────── */

static int pad_note_to_matrix(int note, int *row, int *col) {
    /*
     * Move pads: notes 68-99, 4 rows x 8 cols, bottom-left to top-right.
     * Row 0 = bottom (notes 68-75), Row 3 = top (notes 92-99).
     *
     * For our matrix: Row 0 = Dual OSC, Row 1 = GTO Smooth,
     * Row 2 = GTO Stepped, Row 3 = Wave Mult.
     * We map bottom row (68-75) to Row 0, top row (92-99) to Row 3.
     */
    if (note < PAD_NOTE_BASE || note > 99) return 0;
    int idx = note - PAD_NOTE_BASE;
    *row = idx / 8;
    *col = idx % 8;
    return 1;
}

/* ── Lifecycle ───────────────────────────────────────────────────────────── */

static void *create_instance(const char *module_dir, const char *json_defaults) {
    (void)module_dir; (void)json_defaults;
    serge_instance_t *inst = calloc(1, sizeof(serge_instance_t));
    if (!inst) return NULL;

    /* Encoder defaults */
    inst->enc_osc1_pitch     = 0.0f;   /* C3 */
    inst->enc_osc1_timbre    = 0.5f;
    inst->enc_osc2_pitch     = 0.0f;   /* C3 */
    inst->enc_osc2_harmonics = 0.5f;
    inst->enc_fold_depth     = 0.0f;
    inst->enc_fold_type      = 0.5f;
    inst->enc_filter_cutoff  = 1.0f;   /* fully open (20kHz) */
    inst->enc_filter_q       = 0.0f;

    /* Menu defaults */
    inst->gto_rate    = 1.0f;
    inst->attack      = 0.01f;
    inst->decay       = 0.3f;
    inst->sustain     = 0.7f;
    inst->release     = 0.2f;
    inst->osc_mix     = 0.5f;
    inst->portamento  = 0.0f;
    inst->legato      = 0;
    inst->lfo_waveform = 0;

    /* GTO RNG seed */
    inst->gto.rng_state = 42u;

    return inst;
}

static void destroy_instance(void *instance) {
    free(instance);
}

/* ── MIDI ────────────────────────────────────────────────────────────────── */

static void on_midi(void *instance, const uint8_t *msg, int len, int source) {
    (void)source;
    serge_instance_t *inst = (serge_instance_t *)instance;
    if (len < 3) return;

    uint8_t status = msg[0] & 0xF0;
    uint8_t data1  = msg[1];
    uint8_t data2  = msg[2];

    if (status == 0x90 && data2 > 0) {
        /* Note On — check if it's a pad (modulation matrix) */
        int row, col;
        if (pad_note_to_matrix(data1, &row, &col)) {
            /* Pad press: set modulation depth from velocity */
            inst->mod_matrix[row][col] = data2 / 127.0f;
            return;
        }

        /* Regular note-on: trigger voice */
        inst->midi_note = data1;
        inst->velocity  = data2 / 127.0f;

        if (!inst->legato || !inst->note_active) {
            inst->env.stage = 0; /* attack */
            inst->env.level = 0.0f;
        }
        inst->note_active = 1;

    } else if (status == 0x80 || (status == 0x90 && data2 == 0)) {
        /* Note Off — check pads */
        int row, col;
        if (pad_note_to_matrix(data1, &row, &col)) {
            /* Pad release: clear modulation */
            inst->mod_matrix[row][col] = 0.0f;
            return;
        }

        /* Regular note-off */
        if (data1 == inst->midi_note) {
            inst->env.stage = 3; /* release */
        }

    } else if (status == 0xE0) {
        /* Pitch bend: 14-bit, center=8192, ±2 semitones */
        int bend = (data2 << 7) | data1;
        inst->pitch_bend = (bend - 8192) / 8192.0f * 2.0f;

    } else if (status == 0xB0 && data1 == 1) {
        /* Mod wheel */
        inst->mod_wheel = data2 / 127.0f;
    }
}

/* ── Parameters ──────────────────────────────────────────────────────────── */

static void set_param(void *instance, const char *key, const char *val) {
    serge_instance_t *inst = (serge_instance_t *)instance;
    float f = (float)atof(val);

    /* Encoder params */
    if      (strcmp(key, "osc1_pitch")     == 0) inst->enc_osc1_pitch     = f;
    else if (strcmp(key, "osc1_timbre")    == 0) inst->enc_osc1_timbre    = fmaxf(0, fminf(1, f));
    else if (strcmp(key, "osc2_pitch")     == 0) inst->enc_osc2_pitch     = f;
    else if (strcmp(key, "osc2_harmonics") == 0) inst->enc_osc2_harmonics = fmaxf(0, fminf(1, f));
    else if (strcmp(key, "fold_depth")     == 0) inst->enc_fold_depth     = fmaxf(0, fminf(1, f));
    else if (strcmp(key, "fold_type")      == 0) inst->enc_fold_type      = fmaxf(0, fminf(1, f));
    else if (strcmp(key, "filter_cutoff")  == 0) inst->enc_filter_cutoff  = fmaxf(0, fminf(1, f));
    else if (strcmp(key, "filter_q")       == 0) inst->enc_filter_q       = fmaxf(0, fminf(1, f));
    /* Menu params */
    else if (strcmp(key, "gto_rate")       == 0) inst->gto_rate    = fmaxf(0.01f, fminf(40, f));
    else if (strcmp(key, "attack")         == 0) inst->attack      = fmaxf(0.001f, fminf(10, f));
    else if (strcmp(key, "decay")          == 0) inst->decay       = fmaxf(0.001f, fminf(10, f));
    else if (strcmp(key, "sustain")        == 0) inst->sustain     = fmaxf(0, fminf(1, f));
    else if (strcmp(key, "release")        == 0) inst->release     = fmaxf(0.001f, fminf(10, f));
    else if (strcmp(key, "osc_mix")        == 0) inst->osc_mix     = fmaxf(0, fminf(1, f));
    else if (strcmp(key, "portamento")     == 0) inst->portamento  = fmaxf(0, fminf(2, f));
    else if (strcmp(key, "legato")         == 0) inst->legato      = (int)f;
    else if (strcmp(key, "lfo_waveform")   == 0) inst->lfo_waveform = (int)f;
}

static int get_param(void *instance, const char *key, char *buf, int buf_len) {
    serge_instance_t *inst = (serge_instance_t *)instance;

    if (strcmp(key, "chain_params") == 0) {
        const char *json =
            "["
            /* Oscillators */
            "{\"key\":\"osc1_pitch\",\"name\":\"OSC1 Pitch\",\"type\":\"float\",\"min\":-2,\"max\":5,\"step\":0.01,\"default\":0},"
            "{\"key\":\"osc1_timbre\",\"name\":\"OSC1 Timbre\",\"type\":\"float\",\"min\":0,\"max\":1,\"step\":0.01,\"default\":0.5},"
            "{\"key\":\"osc2_pitch\",\"name\":\"OSC2 Pitch\",\"type\":\"float\",\"min\":-2,\"max\":5,\"step\":0.01,\"default\":0},"
            "{\"key\":\"osc2_harmonics\",\"name\":\"OSC2 Harmonics\",\"type\":\"float\",\"min\":0,\"max\":1,\"step\":0.01,\"default\":0.5},"
            "{\"key\":\"osc_mix\",\"name\":\"OSC Mix\",\"type\":\"float\",\"min\":0,\"max\":1,\"step\":0.01,\"default\":0.5},"
            /* Wavefold */
            "{\"key\":\"fold_depth\",\"name\":\"Fold Depth\",\"type\":\"float\",\"min\":0,\"max\":1,\"step\":0.01,\"default\":0},"
            "{\"key\":\"fold_type\",\"name\":\"Fold Type\",\"type\":\"float\",\"min\":0,\"max\":1,\"step\":0.01,\"default\":0.5},"
            /* Filter */
            "{\"key\":\"filter_cutoff\",\"name\":\"Cutoff\",\"type\":\"float\",\"min\":0,\"max\":1,\"step\":0.01,\"default\":1},"
            "{\"key\":\"filter_q\",\"name\":\"Q\",\"type\":\"float\",\"min\":0,\"max\":1,\"step\":0.01,\"default\":0},"
            /* Envelope */
            "{\"key\":\"attack\",\"name\":\"Attack\",\"type\":\"float\",\"min\":0.001,\"max\":10,\"step\":0.001,\"default\":0.01},"
            "{\"key\":\"decay\",\"name\":\"Decay\",\"type\":\"float\",\"min\":0.001,\"max\":10,\"step\":0.001,\"default\":0.3},"
            "{\"key\":\"sustain\",\"name\":\"Sustain\",\"type\":\"float\",\"min\":0,\"max\":1,\"step\":0.01,\"default\":0.7},"
            "{\"key\":\"release\",\"name\":\"Release\",\"type\":\"float\",\"min\":0.001,\"max\":10,\"step\":0.001,\"default\":0.2},"
            /* Modulation */
            "{\"key\":\"gto_rate\",\"name\":\"GTO Rate\",\"type\":\"float\",\"min\":0.01,\"max\":40,\"step\":0.01,\"default\":1,\"unit\":\"Hz\"},"
            "{\"key\":\"lfo_waveform\",\"name\":\"LFO Wave\",\"type\":\"enum\",\"options\":[\"Triangle\",\"Sine\",\"Saw\"],\"default\":\"Triangle\"},"
            "{\"key\":\"portamento\",\"name\":\"Portamento\",\"type\":\"float\",\"min\":0,\"max\":2,\"step\":0.01,\"default\":0,\"unit\":\"s\"},"
            "{\"key\":\"legato\",\"name\":\"Legato\",\"type\":\"enum\",\"options\":[\"Off\",\"On\"],\"default\":\"Off\"}"
            "]";
        strncpy(buf, json, buf_len - 1);
        buf[buf_len - 1] = '\0';
        return (int)strlen(json);
    }

    /* Individual param getters */
    if (strcmp(key, "osc1_pitch")     == 0) return snprintf(buf, buf_len, "%.3f", inst->enc_osc1_pitch);
    if (strcmp(key, "osc1_timbre")    == 0) return snprintf(buf, buf_len, "%.3f", inst->enc_osc1_timbre);
    if (strcmp(key, "osc2_pitch")     == 0) return snprintf(buf, buf_len, "%.3f", inst->enc_osc2_pitch);
    if (strcmp(key, "osc2_harmonics") == 0) return snprintf(buf, buf_len, "%.3f", inst->enc_osc2_harmonics);
    if (strcmp(key, "fold_depth")     == 0) return snprintf(buf, buf_len, "%.3f", inst->enc_fold_depth);
    if (strcmp(key, "fold_type")      == 0) return snprintf(buf, buf_len, "%.3f", inst->enc_fold_type);
    if (strcmp(key, "filter_cutoff")  == 0) return snprintf(buf, buf_len, "%.3f", inst->enc_filter_cutoff);
    if (strcmp(key, "filter_q")       == 0) return snprintf(buf, buf_len, "%.3f", inst->enc_filter_q);
    if (strcmp(key, "gto_rate")       == 0) return snprintf(buf, buf_len, "%.3f", inst->gto_rate);
    if (strcmp(key, "attack")         == 0) return snprintf(buf, buf_len, "%.3f", inst->attack);
    if (strcmp(key, "decay")          == 0) return snprintf(buf, buf_len, "%.3f", inst->decay);
    if (strcmp(key, "sustain")        == 0) return snprintf(buf, buf_len, "%.3f", inst->sustain);
    if (strcmp(key, "release")        == 0) return snprintf(buf, buf_len, "%.3f", inst->release);
    if (strcmp(key, "osc_mix")        == 0) return snprintf(buf, buf_len, "%.3f", inst->osc_mix);
    if (strcmp(key, "portamento")     == 0) return snprintf(buf, buf_len, "%.3f", inst->portamento);
    if (strcmp(key, "legato")         == 0) return snprintf(buf, buf_len, "%d", inst->legato);
    if (strcmp(key, "lfo_waveform")   == 0) return snprintf(buf, buf_len, "%d", inst->lfo_waveform);

    return -1;
}

/* ── Audio processing ────────────────────────────────────────────────────── */

static void render_block(void *instance, int16_t *out_lr, int frames) {
    serge_instance_t *inst = (serge_instance_t *)instance;

    /* Compute modulated parameters once per block (from last block's mod sources) */
    modulated_params_t mp;
    compute_modulated_params(inst, &mp);

    /* Oscillator frequencies: CV + pitch bend */
    float osc1_hz = cv_to_freq(mp.osc1_pitch_cv + inst->pitch_bend / 12.0f);
    float osc2_hz = cv_to_freq(mp.osc2_pitch_cv + inst->pitch_bend / 12.0f);

    for (int i = 0; i < frames; i++) {
        float sample = 0.0f;

        /* GTO+ tick (always running, even when voice is off) */
        float gto_smooth, gto_stepped;
        gto_tick(&inst->gto, inst->gto_rate, &gto_smooth, &gto_stepped);

        if (inst->note_active || inst->env.stage == 3) {
            /* Generate oscillators */
            float s1 = complex_osc_sample(&inst->osc1, osc1_hz, mp.osc1_timbre);
            float s2 = nto_sample(&inst->osc2, osc2_hz, mp.osc2_harmonics);

            /* Mix oscillators */
            float mixed = s1 * (1.0f - inst->osc_mix) + s2 * inst->osc_mix;

            /* Wave Multiplier */
            float folded = wavefold(mixed, mp.fold_depth, mp.fold_type);

            /* VCFQ+ filter */
            float filtered = vcfq_process(&inst->filter, folded,
                                          mp.filter_cutoff_hz, inst->enc_filter_q);

            /* ADSR envelope */
            float env = adsr_tick(&inst->env, inst->attack, inst->decay,
                                  inst->sustain, inst->release);

            /* VCA: envelope * velocity * output level modulation */
            sample = filtered * env * inst->velocity * mp.output_level * 0.7f;

            /* Update modulation sources for next block's matrix computation */
            inst->mod_src[SRC_DUAL_OSC]   = (s1 + s2) * 0.5f;
            inst->mod_src[SRC_WAVE_MULT]  = folded;

            if (inst->env.stage == 4) {
                inst->note_active = 0;
            }
        }

        /* GTO sources always update (modulation runs even without audio) */
        inst->mod_src[SRC_GTO_SMOOTH]  = gto_smooth;
        inst->mod_src[SRC_GTO_STEPPED] = gto_stepped;

        /* Clamp and write stereo int16 */
        sample = fmaxf(-1.0f, fminf(1.0f, sample));
        int16_t s = (int16_t)(sample * 32767.0f);
        out_lr[i * 2]     = s;
        out_lr[i * 2 + 1] = s;
    }
}

/* ── API v2 export ───────────────────────────────────────────────────────── */

typedef struct {
    uint32_t api_version;
    void* (*create_instance)(const char *, const char *);
    void  (*destroy_instance)(void *);
    void  (*on_midi)(void *, const uint8_t *, int, int);
    void  (*set_param)(void *, const char *, const char *);
    int   (*get_param)(void *, const char *, char *, int);
    void  (*render_block)(void *, int16_t *, int);
} plugin_api_v2_t;

plugin_api_v2_t* move_plugin_init_v2(const void *host) {
    (void)host;
    static plugin_api_v2_t api = {
        .api_version      = 2,
        .create_instance  = create_instance,
        .destroy_instance = destroy_instance,
        .on_midi          = on_midi,
        .set_param        = set_param,
        .get_param        = get_param,
        .render_block     = render_block,
    };
    return &api;
}
