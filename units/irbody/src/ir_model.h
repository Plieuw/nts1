/*
 * ir_model.h -- impulse response models for the body unit.
 *
 * BSD 3-Clause License, see LICENSE.
 *
 * The responses are built from the things a real body or space does: a handful
 * of resonances at physically plausible frequencies, each with its own damping,
 * a diffuse tail whose high frequencies die away faster than its low ones, a
 * short broadband impact for the knock. They are all bodies: soundboxes, tubes
 * and shells, the things a synthesiser would be played through.
 *
 * The unit builds its filters from these numbers (a resonator per mode, a
 * feedback delay network with the decay and brightness of the tail), and
 * tools/make_irs.cpp renders the same model as WAV files in ir/. The WAV files
 * are the reference: the unit's levels were fitted band by band to them.
 *
 * The renderer below is the WAV generator's, not the unit's. It is behind
 * IRMODEL_RENDER because it is written with sinf/cosf/expf for readability and
 * the unit has 12 KB of code for everything: compiling it into the firmware
 * would pull the maths library in for a function nobody calls there.
 *
 * Mode ratios: violin, cello and guitar use the air and wood resonances of the
 * real instruments (A0, B1-, B1+ and the bridge hill), the wood preset uses the
 * free-free bar series, the steel preset a plate-like inharmonic series.
 * Decay times are chosen so the response has rung out within kIrLen.
 */

#ifndef IR_MODEL_H
#define IR_MODEL_H

#include <stdint.h>

#ifdef IRMODEL_RENDER
#include <math.h>
#endif

namespace irmodel {

static const int kIrLen = 4096;  // 85.3 ms at 48 kHz
static const int kPresetCount = 8;
static const float kFs = 48000.f;
static const float kPi = 3.14159265358979f;

struct Mode {
  float ratio;  // frequency, relative to the preset fundamental
  float amp;    // level relative to the first mode
  float decay;  // multiplier on the preset decay time
};

struct Preset {
  const char *name;
  const Mode *modes;
  int mode_count;
  float f0;         // fundamental of the mode set, Hz (0 for pure spaces)
  float tau;        // e-folding decay of the lowest mode, seconds
  float noise;      // amount of diffuse tail
  float noise_tau;  // e-folding decay of the diffuse tail, seconds
  float bright;     // brightness of the diffuse tail, 0..1
  float impact;     // broadband knock at t = 0
};

// --- violin body: 280 Hz air resonance, 470/550 Hz wood, 2.3-3.5 kHz bridge hill
static const Mode kViolin[] = {
    {1.00f, 1.00f, 1.00f}, {1.36f, 0.35f, 0.80f}, {1.68f, 0.55f, 0.90f},
    {1.96f, 0.50f, 0.85f}, {2.50f, 0.40f, 0.70f}, {3.75f, 0.30f, 0.60f},
    {5.70f, 0.35f, 0.50f}, {8.20f, 0.30f, 0.45f}, {9.60f, 0.28f, 0.40f},
    {11.4f, 0.22f, 0.35f}, {12.5f, 0.18f, 0.30f},
};

// --- cello body: 105 Hz air resonance, 180/220 Hz wood, bridge hill lower
static const Mode kCello[] = {
    {1.00f, 1.00f, 1.00f}, {1.71f, 0.60f, 0.85f}, {2.10f, 0.55f, 0.80f},
    {2.86f, 0.45f, 0.70f}, {4.57f, 0.40f, 0.60f}, {7.14f, 0.35f, 0.50f},
    {11.0f, 0.30f, 0.45f}, {15.7f, 0.25f, 0.38f}, {20.0f, 0.18f, 0.30f},
};

// --- piano: soundboard modes, lowest around 70 Hz, dense and fairly damped
static const Mode kPiano[] = {
    {1.00f, 1.00f, 1.00f}, {1.50f, 0.70f, 0.90f}, {2.07f, 0.60f, 0.85f},
    {2.71f, 0.55f, 0.80f}, {3.57f, 0.50f, 0.72f}, {4.57f, 0.45f, 0.65f},
    {6.00f, 0.40f, 0.58f}, {8.00f, 0.35f, 0.50f}, {11.1f, 0.30f, 0.44f},
    {15.0f, 0.25f, 0.38f}, {21.4f, 0.18f, 0.32f},
};

// --- acoustic guitar: 100 Hz Helmholtz resonance, 200 Hz top, then wood modes
static const Mode kGuitar[] = {
    {1.00f, 1.00f, 1.00f}, {2.00f, 0.65f, 0.85f}, {2.40f, 0.45f, 0.80f},
    {3.30f, 0.40f, 0.72f}, {4.70f, 0.35f, 0.62f}, {6.60f, 0.32f, 0.55f},
    {9.00f, 0.25f, 0.45f}, {12.5f, 0.20f, 0.38f}, {17.0f, 0.15f, 0.30f},
};

// --- marimba bar: free-free bar series 1 : 2.756 : 5.404 : 8.933 : 13.34 : 18.64
static const Mode kWood[] = {
    {1.000f, 1.00f, 1.00f}, {2.756f, 0.55f, 0.70f}, {5.404f, 0.35f, 0.55f},
    {8.933f, 0.22f, 0.42f}, {13.34f, 0.14f, 0.32f}, {18.64f, 0.08f, 0.25f},
};

// --- steel plate: inharmonic, long and even decay
static const Mode kSteel[] = {
    {1.00f, 1.00f, 1.00f},  {2.39f, 0.80f, 0.95f}, {3.86f, 0.70f, 0.90f},
    {5.62f, 0.60f, 0.85f},  {7.71f, 0.50f, 0.80f}, {10.1f, 0.42f, 0.75f},
    {12.9f, 0.34f, 0.70f},  {16.1f, 0.26f, 0.62f},
};

// --- clarinet body: a closed-open tube, so the odd harmonics of its air column,
// with the damping a wooden bore puts on the higher ones.
static const Mode kClarinet[] = {
    {1.000f, 1.00f, 1.00f}, {3.000f, 0.55f, 0.80f}, {5.000f, 0.40f, 0.62f},
    {7.000f, 0.30f, 0.50f}, {9.000f, 0.22f, 0.40f}, {11.00f, 0.16f, 0.32f},
    {13.00f, 0.12f, 0.26f}, {15.00f, 0.09f, 0.21f}, {17.00f, 0.07f, 0.17f},
};

// --- drum shell: the few modes of a cylindrical shell, low, with a lot of knock
// and a short breath of noise around the rim.
static const Mode kDrum[] = {
    {1.000f, 1.00f, 1.00f}, {1.510f, 0.80f, 0.85f}, {1.980f, 0.60f, 0.70f},
    {2.440f, 0.45f, 0.58f}, {2.980f, 0.32f, 0.46f}, {3.600f, 0.22f, 0.36f},
    {4.300f, 0.15f, 0.28f},
};

static const Preset kPresets[kPresetCount] = {
    // name      modes    count  f0     tau     noise  n_tau   bright impact
    {"violin", kViolin, 11, 280.f, 0.020f, 0.12f, 0.008f, 0.60f, 0.35f},
    {"cello", kCello, 9, 105.f, 0.024f, 0.12f, 0.009f, 0.55f, 0.35f},
    {"piano", kPiano, 11, 70.f, 0.030f, 0.20f, 0.014f, 0.45f, 0.30f},
    {"guitar", kGuitar, 9, 100.f, 0.022f, 0.15f, 0.010f, 0.50f, 0.30f},
    {"wood", kWood, 6, 320.f, 0.010f, 0.20f, 0.004f, 0.62f, 0.45f},
    {"steel", kSteel, 8, 430.f, 0.035f, 0.25f, 0.020f, 0.75f, 0.35f},
    {"clarinet", kClarinet, 9, 131.f, 0.022f, 0.15f, 0.010f, 0.45f, 0.30f},
    {"drum", kDrum, 7, 90.f, 0.028f, 0.35f, 0.013f, 0.55f, 0.60f},
};

#ifdef IRMODEL_RENDER
static uint32_t s_rng;

static inline float rand01() {
  s_rng = s_rng * 1664525u + 1013904223u;
  return (float)(s_rng >> 8) * (1.f / 16777216.f);
}

static inline float randbi() {
  return rand01() * 2.f - 1.f;
}

/* Renders one response into h, normalised and faded out at the end so the
 * truncation at kIrLen is inaudible. */
static void render(float *h, int preset) {
  const Preset &p = kPresets[preset];

  for (int i = 0; i < kIrLen; ++i)
    h[i] = 0.f;

  s_rng = 0x2545F491u + 7919u * (uint32_t)preset;

  // Resonances: a decaying sine per mode, started from an impulse.
  for (int k = 0; k < p.mode_count; ++k) {
    const Mode &m = p.modes[k];
    const float f = p.f0 * m.ratio;
    if (f >= 0.45f * kFs)
      continue;

    const float r = expf(-1.f / (p.tau * m.decay * kFs));
    const float w = 2.f * kPi * f / kFs;
    const float c = 2.f * r * cosf(w);
    const float r2 = r * r;

    float y0 = 0.f;
    float y1 = m.amp * sinf(w);
    for (int n = 0; n < kIrLen; ++n) {
      h[n] += y1;
      const float y2 = c * y1 - r2 * y0;
      y0 = y1;
      y1 = y2;
    }
  }

  // Knock: a couple of milliseconds of broadband noise.
  if (p.impact > 0.f) {
    const float a = 1.f - expf(-2.f * kPi * 1800.f / kFs);
    const float decay = expf(-1.f / (0.0015f * kFs));
    float lp = 0.f;
    float env = 1.f;
    const int len = (kIrLen < 300) ? kIrLen : 300;
    for (int n = 0; n < len; ++n) {
      lp += (randbi() - lp) * a;
      h[n] += lp * env * p.impact;
      env *= decay;
    }
  }

  // Diffuse tail: low and high band, the high band dying away faster.
  if (p.noise > 0.f) {
    const float a_lo = 1.f - expf(-2.f * kPi * 450.f / kFs);
    const float c_lo = expf(-1.f / (p.noise_tau * kFs));
    const float c_hi = expf(-1.f / (p.noise_tau * 0.45f * kFs));
    const float hi_gain = 0.35f + 0.9f * p.bright;

    float lo = 0.f;
    float env_lo = 1.f;
    float env_hi = 1.f;
    for (int n = 0; n < kIrLen; ++n) {
      const float w = randbi();
      lo += (w - lo) * a_lo;
      h[n] += (lo * env_lo + (w - lo) * env_hi * hi_gain) * p.noise * 1.6f;
      env_lo *= c_lo;
      env_hi *= c_hi;
    }

    // Early reflections give the spaces their size.
    if (p.mode_count == 0) {
      for (int r = 0; r < 10; ++r) {
        const int d = (int)((0.002f + 0.013f * rand01()) * kFs);
        if (d < kIrLen)
          h[d] += randbi() * 0.7f * expf(-0.25f * (float)r);
      }
    }
  }

  float peak = 1e-6f;
  for (int i = 0; i < kIrLen; ++i)
    peak = fmaxf(peak, fabsf(h[i]));

  const float sc = 0.5f / peak;
  const int fade = 512;
  for (int i = 0; i < kIrLen; ++i) {
    float g = 1.f;
    if (i >= kIrLen - fade) {
      const float x = (float)(kIrLen - 1 - i) / (float)fade;
      g = 0.5f - 0.5f * cosf(kPi * x);
    }
    h[i] *= sc * g;
  }
}
#endif  // IRMODEL_RENDER

}  // namespace irmodel

#endif  // IR_MODEL_H
