/*
 * body.cpp -- impulse response (body / space) convolution for the
 * KORG Nu:Tekt NTS-1.
 *
 * BSD 3-Clause License, see LICENSE.
 *
 * Runs in the DELAY slot and convolves the incoming sound with a short impulse
 * response, so the synth sits inside a violin body, a piano soundboard, a metal
 * pipe or a small space instead of on its own. The responses are generated at
 * load time from a handful of damped modes plus a diffuse noise tail, which
 * keeps the unit free of sampled material while still giving true convolution
 * behaviour: every partial of the source is coloured the same way a real body
 * would colour it.
 *
 * The convolution is partitioned in the frequency domain (8 x 256 taps, FFT
 * size 512), which costs a fraction of what direct convolution of the same
 * 42.7 ms response would need.
 *
 * A-knob (TIME)   which response: violin, cello, piano, guitar, wood, steel,
 *                 room, plate
 * B-knob (DEPTH)  damping of the wet signal, dark .. open
 * DEL + B knob    dry/wet mix
 */

#include "userdelfx.h"

namespace {

constexpr float kFs = 48000.f;
constexpr float kPi = 3.14159265358979f;

constexpr int kFFT = 512;             // FFT size
constexpr int kHop = 256;             // samples per block (5.3 ms latent)
constexpr int kParts = 8;             // -> 2048 taps, 42.7 ms of response
constexpr int kIrLen = kParts * kHop;

struct Preset {
  float f0;      // modal fundamental
  float tau;     // decay time
  float noise;   // amount of diffuse tail
  float bright;  // brightness of the diffuse tail
};

const Preset kPresets[] = {
    {260.f, 0.14f, 0.10f, 0.55f},  // violin body
    {120.f, 0.20f, 0.08f, 0.45f},  // cello body
    { 80.f, 0.30f, 0.12f, 0.40f},  // piano soundboard
    { 98.f, 0.22f, 0.10f, 0.50f},  // acoustic guitar
    {330.f, 0.07f, 0.18f, 0.62f},  // wood / marimba bar
    {430.f, 0.45f, 0.22f, 0.78f},  // steel / pipe
    {110.f, 0.35f, 1.00f, 0.35f},  // small room
    {140.f, 0.55f, 1.00f, 0.68f},  // plate
};

constexpr int kPresetCount = (int)(sizeof(kPresets) / sizeof(kPresets[0]));

// The .sdram section is not zeroed at load time, so everything here is cleared
// explicitly in DELFX_INIT().
float g_ir[kIrLen] __sdram;
float g_twre[kFFT / 2] __sdram;
float g_twim[kFFT / 2] __sdram;
float g_in[kFFT] __sdram;
float g_re[kFFT] __sdram;
float g_im[kFFT] __sdram;
float g_xre[kParts][kFFT] __sdram;
float g_xim[kParts][kFFT] __sdram;
float g_hre[kParts][kFFT] __sdram;
float g_him[kParts][kFFT] __sdram;
float g_out[kHop] __sdram;
float g_pending[kHop] __sdram;
float g_dry[kHop] __sdram;

struct State {
  int preset;
  float damp;       // 0..1
  float wet;        // 0..1
  float damp_coef;
  float damp_lp;
  int pos;
  int ring;
  bool need_ir;
};

State s;
uint32_t g_rng;

inline float rnd01() {
  g_rng = g_rng * 1664525u + 1013904223u;
  return (float)(g_rng >> 8) * (1.f / 16777216.f);
}

inline float rndbi() {
  return rnd01() * 2.f - 1.f;
}

/*---------------------------------------------------------------------------*
 * FFT
 *---------------------------------------------------------------------------*/

void fft(float *re, float *im, bool inverse) {
  for (int i = 1, j = 0; i < kFFT; ++i) {
    int bit = kFFT >> 1;
    for (; j & bit; bit >>= 1)
      j ^= bit;
    j ^= bit;
    if (i < j) {
      const float tr = re[i];
      re[i] = re[j];
      re[j] = tr;
      const float ti = im[i];
      im[i] = im[j];
      im[j] = ti;
    }
  }

  for (int len = 2; len <= kFFT; len <<= 1) {
    const int half = len >> 1;
    const int step = kFFT / len;
    for (int base = 0; base < kFFT; base += len) {
      for (int j = 0; j < half; ++j) {
        const int k = j * step;
        const float wr = g_twre[k];
        const float wi = inverse ? -g_twim[k] : g_twim[k];
        const int a = base + j;
        const int b = a + half;
        const float xr = re[b] * wr - im[b] * wi;
        const float xi = re[b] * wi + im[b] * wr;
        re[b] = re[a] - xr;
        im[b] = im[a] - xi;
        re[a] += xr;
        im[a] += xi;
      }
    }
  }

  if (inverse) {
    const float sc = 1.f / (float)kFFT;
    for (int i = 0; i < kFFT; ++i) {
      re[i] *= sc;
      im[i] *= sc;
    }
  }
}

/*---------------------------------------------------------------------------*
 * Response generation
 *---------------------------------------------------------------------------*/

void buildIr(int preset) {
  const Preset &p = kPresets[preset];

  for (int i = 0; i < kIrLen; ++i)
    g_ir[i] = 0.f;

  g_rng = 0x2F6E2B1u + (uint32_t)preset * 7919u;

  // Modal part: damped sinusoids, spread out and slightly irregular so the
  // body has density instead of a handful of obvious resonances.
  for (int k = 0; k < 14; ++k) {
    const float jitter = 0.85f + 0.45f * rnd01();
    const float f = p.f0 * (1.f + 1.85f * (float)k * jitter);
    if (f > 0.45f * kFs)
      break;

    const float amp = 0.8f * expf(-0.32f * (float)k) * (0.6f + 0.5f * rnd01());
    const float tau = p.tau * expf(-0.30f * (float)k);
    const float r = expf(-1.f / (tau * kFs));
    const float w = 2.f * kPi * f / kFs;
    const float c = 2.f * r * cosf(w);
    const float r2 = r * r;

    float y0 = 0.f;
    float y1 = amp * sinf(w);
    for (int n = 0; n < kIrLen; ++n) {
      g_ir[n] += y1;
      const float y2 = c * y1 - r2 * y0;
      y0 = y1;
      y1 = y2;
    }
  }

  // Diffuse part: a decaying, filtered noise tail plus a few early reflections.
  if (p.noise > 0.f) {
    const float a = clipminmaxf(0.02f, p.bright * 0.9f, 0.99f);
    float lp = 0.f;
    for (int n = 0; n < kIrLen; ++n) {
      lp += (rndbi() - lp) * a;
      g_ir[n] += lp * expf(-(float)n / (p.tau * kFs)) * p.noise * 0.6f;
    }

    for (int r = 0; r < 8; ++r) {
      const int d = 24 + (int)(rnd01() * 0.012f * kFs);
      if (d < kIrLen)
        g_ir[d] += rndbi() * 0.5f * expf(-0.35f * (float)r);
    }
  }

  float peak = 1e-6f;
  for (int i = 0; i < kIrLen; ++i)
    peak = fmaxf(peak, fabsf(g_ir[i]));

  const float sc = 0.5f / peak;
  for (int i = 0; i < kIrLen; ++i)
    g_ir[i] *= sc;
}

void computePartitions() {
  for (int p = 0; p < kParts; ++p) {
    for (int i = 0; i < kFFT; ++i) {
      g_re[i] = 0.f;
      g_im[i] = 0.f;
    }
    for (int i = 0; i < kHop; ++i)
      g_re[i] = g_ir[p * kHop + i];

    fft(g_re, g_im, false);

    for (int i = 0; i < kFFT; ++i) {
      g_hre[p][i] = g_re[i];
      g_him[p][i] = g_im[i];
    }
  }
}

/*---------------------------------------------------------------------------*
 * Convolution block
 *---------------------------------------------------------------------------*/

void runBlock() {
  if (s.need_ir) {
    buildIr(s.preset);
    computePartitions();
    s.need_ir = false;
  }

  for (int i = 0; i < kFFT - kHop; ++i)
    g_in[i] = g_in[i + kHop];
  for (int i = 0; i < kHop; ++i)
    g_in[kFFT - kHop + i] = g_pending[i];

  for (int i = 0; i < kFFT; ++i) {
    g_re[i] = g_in[i];
    g_im[i] = 0.f;
  }
  fft(g_re, g_im, false);

  s.ring = (s.ring + 1) % kParts;
  for (int i = 0; i < kFFT; ++i) {
    g_xre[s.ring][i] = g_re[i];
    g_xim[s.ring][i] = g_im[i];
    g_re[i] = 0.f;
    g_im[i] = 0.f;
  }

  for (int p = 0; p < kParts; ++p) {
    const int idx = (s.ring - p + kParts) % kParts;
    const float *xr = g_xre[idx];
    const float *xi = g_xim[idx];
    const float *hr = g_hre[p];
    const float *hi = g_him[p];

    for (int i = 0; i < kFFT; ++i) {
      g_re[i] += hr[i] * xr[i] - hi[i] * xi[i];
      g_im[i] += hr[i] * xi[i] + hi[i] * xr[i];
    }
  }

  fft(g_re, g_im, true);

  for (int i = 0; i < kHop; ++i)
    g_out[i] = g_re[kFFT - kHop + i];
}

void updateDamping() {
  const float fc = 1200.f * expf(2.71f * s.damp);  // 1.2 kHz .. 18 kHz
  s.damp_coef = 1.f - expf(-2.f * kPi * clipminmaxf(200.f, fc, 0.45f * kFs) / kFs);
}

void clearBuffers() {
  for (int i = 0; i < kFFT; ++i) {
    g_in[i] = 0.f;
    g_re[i] = 0.f;
    g_im[i] = 0.f;
  }
  for (int p = 0; p < kParts; ++p) {
    for (int i = 0; i < kFFT; ++i) {
      g_xre[p][i] = 0.f;
      g_xim[p][i] = 0.f;
      g_hre[p][i] = 0.f;
      g_him[p][i] = 0.f;
    }
  }
  for (int i = 0; i < kHop; ++i) {
    g_out[i] = 0.f;
    g_pending[i] = 0.f;
    g_dry[i] = 0.f;
  }
  s.pos = 0;
  s.ring = 0;
  s.damp_lp = 0.f;
}

}  // namespace

/*---------------------------------------------------------------------------*
 * Unit hooks
 *---------------------------------------------------------------------------*/

void DELFX_INIT(uint32_t platform, uint32_t api) {
  (void)platform;
  (void)api;

  for (int i = 0; i < kFFT / 2; ++i) {
    const float a = 2.f * kPi * (float)i / (float)kFFT;
    g_twre[i] = cosf(a);
    g_twim[i] = -sinf(a);
  }

  s.preset = 0;
  s.damp = 0.7f;
  s.wet = 0.5f;
  s.need_ir = false;

  updateDamping();
  clearBuffers();
  buildIr(s.preset);
  computePartitions();
}

void DELFX_PROCESS(float *xn, uint32_t frames) {
  const int nframes = (int)frames;

  for (int i = 0; i < nframes; ++i) {
    const float l = xn[2 * i];
    const float r = xn[2 * i + 1];
    const float in = 0.5f * (l + r);

    const float wet_raw = g_out[s.pos];
    const float dry = g_dry[s.pos];

    s.damp_lp += (wet_raw - s.damp_lp) * s.damp_coef;
    const float wet = s.damp_lp;

    g_dry[s.pos] = in;
    g_pending[s.pos] = in;

    const float y = dry + (wet - dry) * s.wet;
    xn[2 * i] = fx_softclipf(0.05f, y);
    xn[2 * i + 1] = xn[2 * i];

    if (++s.pos >= kHop) {
      s.pos = 0;
      runBlock();
    }
  }
}

void DELFX_PARAM(uint8_t index, int32_t value) {
  switch (index) {
  case k_user_delfx_param_time: {
    const int sel = (int)clipminmaxf(
        0.f, (float)value * (1.f / 1023.f) * (float)kPresetCount, (float)(kPresetCount - 1));
    if (sel != s.preset) {
      s.preset = sel;
      s.need_ir = true;
    }
    break;
  }

  case k_user_delfx_param_depth:
    s.damp = clipminmaxf(0.f, (float)value * (1.f / 1023.f), 1.f);
    updateDamping();
    break;

  case k_user_delfx_param_shift_depth:
    s.wet = clipminmaxf(0.f, (float)value * (1.f / 1023.f), 1.f);
    break;

  default:
    break;
  }
}

void DELFX_SUSPEND(void) {
  clearBuffers();
}

void DELFX_RESUME(void) {}
