/*
 * body.cpp -- impulse response (body / space) convolution for the
 * KORG Nu:Tekt NTS-1.
 *
 * BSD 3-Clause License, see LICENSE.
 *
 * Runs in the DELAY slot and convolves the incoming sound with a short impulse
 * response, so the synth sits inside a violin body, a piano soundboard, a metal
 * plate or a small space instead of on its own. The responses are modelled in
 * ir_model.h from resonances, a diffuse tail and early reflections; the same
 * header writes them out as WAV files in ir/ through tools/make_irs.cpp.
 *
 * The convolution is partitioned in the frequency domain (16 x 256 taps, FFT
 * size 512), which costs a fraction of what direct convolution of the same
 * 85 ms response would need. Only the real halves of the spectra are kept out
 * of the product sum, everything stays in SDRAM.
 *
 * A-knob (TIME)   which response: violin, cello, piano, guitar, wood, steel,
 *                 room, plate
 * B-knob (DEPTH)  damping of the wet signal, dark .. open
 * DEL + B knob    dry/wet mix
 */

#include "userdelfx.h"

#include "ir_model.h"

namespace {

constexpr float kFs = 48000.f;

constexpr int kFFT = 512;                     // FFT size
constexpr int kHop = 256;                     // samples per block (5.3 ms latent)
constexpr int kParts = 16;                    // -> 4096 taps, 85.3 ms
constexpr int kIrLen = irmodel::kIrLen;
constexpr int kPresetCount = irmodel::kPresetCount;

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
 * Convolution
 *---------------------------------------------------------------------------*/

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

void runBlock() {
  if (s.need_ir) {
    irmodel::render(g_ir, s.preset);
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
  s.damp_coef = 1.f - expf(-2.f * 3.14159265358979f *
                           clipminmaxf(200.f, fc, 0.45f * kFs) / kFs);
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
    const float a = 2.f * 3.14159265358979f * (float)i / (float)kFFT;
    g_twre[i] = cosf(a);
    g_twim[i] = -sinf(a);
  }

  s.preset = 0;
  s.damp = 0.7f;
  s.wet = 0.5f;
  s.need_ir = false;

  updateDamping();
  clearBuffers();
  irmodel::render(g_ir, s.preset);
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
        0.f, (float)value * (1.f / 1023.f) * (float)kPresetCount,
        (float)(kPresetCount - 1));
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
