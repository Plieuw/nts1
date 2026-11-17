/*
 * irbody.cpp -- impulse response (body / space) convolution for the
 * KORG Nu:Tekt NTS-1.
 *
 * BSD 3-Clause License, see LICENSE.
 *
 * Runs in the REVERB slot and convolves the incoming sound with a short impulse
 * response, so the synth sits inside a violin body, a piano soundboard, a metal
 * plate or a small space instead of on its own. That is where a space belongs:
 * the reverb slot is the last stage of the NTS-1 chain, so the oscillator, the
 * modulation units and the delay are all already in the sound that reaches it.
 * It is also the slot that fits: a reverb unit gets the same 12 KB of code and
 * 2.4 MB of SDRAM as a delay unit, and the same shift-depth knob for dry/wet.
 * The delay slot stays free for the barberpole phaser or for an echo.
 *
 * The responses are modelled in ir_model.h from resonances, a diffuse tail and
 * early reflections; the same header writes them out as WAV files in ir/ through
 * tools/make_irs.cpp.
 *
 * The convolution is partitioned in the frequency domain (16 x 256 taps, FFT
 * size 512), which costs a fraction of what direct convolution of the same
 * 85 ms response would need.
 *
 * Every response is transformed once at load time and kept in SDRAM, so turning
 * the body knob does not have to build anything in the audio callback. A switch
 * crossfades between two partition sets over 85 ms instead: both convolutions
 * run from the same input spectra and their outputs are mixed, which is exactly
 * a crossfade of the two responses and cannot click.
 *
 * A-knob (TIME)   which response: violin, cello, piano, guitar, wood, steel,
 *                 room, plate
 * B-knob (DEPTH)  damping of the wet signal, dark .. open
 * DEL + B knob    dry/wet mix
 */

#include "userrevfx.h"

#include "ir_model.h"

namespace {

constexpr float kFs = 48000.f;
constexpr float kPi = 3.14159265358979f;

constexpr int kFFT = 512;                     // FFT size
constexpr int kHop = 256;                     // samples per block (5.3 ms latent)
// The input of the convolution is real, so bin i and bin kFFT - i are each
// other's conjugate: only the first half is accumulated and the inverse
// transform is handed the mirror image. The half stops at a power of two so the
// loop still vectorises on a host build; the Nyquist bin is added next to it.
constexpr int kHalfBins = kFFT / 2;
constexpr int kParts = 16;                    // -> 4096 taps, 85.3 ms
constexpr int kIrLen = irmodel::kIrLen;
constexpr int kPresetCount = irmodel::kPresetCount;
constexpr int kFadeSamples = 4096;            // 85 ms crossfade on a switch

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
float g_hre[kPresetCount][kParts][kFFT] __sdram;
float g_him[kPresetCount][kParts][kFFT] __sdram;
float g_out[kHop] __sdram;
float g_pending[kHop] __sdram;
float g_dry[kHop] __sdram;

struct State {
  int active;   // response currently in the output
  int target;   // response fading in, -1 when not fading
  int queued;   // last request that arrived during a fade, -1 when none
  float fade;   // 0..1, position of the crossfade
  float damp;   // 0..1
  float wet;    // 0..1
  float damp_coef;
  float damp_lp;
  int pos;
  int ring;
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
 * Responses
 *---------------------------------------------------------------------------*/

// Transforms one response into its partition spectra. Only ever called from
// DELFX_INIT(), where taking a few milliseconds is fine.
void buildPartitions(int preset) {
  irmodel::render(g_ir, preset);

  for (int p = 0; p < kParts; ++p) {
    for (int i = 0; i < kFFT; ++i) {
      g_re[i] = 0.f;
      g_im[i] = 0.f;
    }
    for (int i = 0; i < kHop; ++i)
      g_re[i] = g_ir[p * kHop + i];

    fft(g_re, g_im, false);

    for (int i = 0; i < kFFT; ++i) {
      g_hre[preset][p][i] = g_re[i];
      g_him[preset][p][i] = g_im[i];
    }
  }
}

// Adds one response's contribution, scaled by w, to the spectrum of the block.
// Only used while two responses are crossfading.
void accumulateWeighted(const float (*hre)[kFFT], const float (*him)[kFFT],
                        float w) {
  if (w == 0.f)
    return;

  for (int p = 0; p < kParts; ++p) {
    int idx = s.ring - p;
    if (idx < 0)
      idx += kParts;
    const float *xr = g_xre[idx];
    const float *xi = g_xim[idx];
    const float *hr = hre[p];
    const float *hi = him[p];

    for (int i = 0; i < kHalfBins; ++i) {
      g_re[i] += w * (hr[i] * xr[i] - hi[i] * xi[i]);
      g_im[i] += w * (hr[i] * xi[i] + hi[i] * xr[i]);
    }
    g_re[kHalfBins] += w * (hr[kHalfBins] * xr[kHalfBins] - hi[kHalfBins] * xi[kHalfBins]);
    g_im[kHalfBins] += w * (hr[kHalfBins] * xi[kHalfBins] + hi[kHalfBins] * xr[kHalfBins]);
  }
}

// The steady state, and the only place the unit spends its time when a knob is
// not turning. Multiplying by one is a no-op, so it gets its own loop: four
// multiplies per bin fewer, and no weight in the loop that the compiler has to
// keep around.
void accumulateUnit(const float (*hre)[kFFT], const float (*him)[kFFT]) {
  for (int p = 0; p < kParts; ++p) {
    int idx = s.ring - p;
    if (idx < 0)
      idx += kParts;
    const float *xr = g_xre[idx];
    const float *xi = g_xim[idx];
    const float *hr = hre[p];
    const float *hi = him[p];

    for (int i = 0; i < kHalfBins; ++i) {
      g_re[i] += hr[i] * xr[i] - hi[i] * xi[i];
      g_im[i] += hr[i] * xi[i] + hi[i] * xr[i];
    }
    g_re[kHalfBins] += hr[kHalfBins] * xr[kHalfBins] - hi[kHalfBins] * xi[kHalfBins];
    g_im[kHalfBins] += hr[kHalfBins] * xi[kHalfBins] + hi[kHalfBins] * xr[kHalfBins];
  }
}

// Fills the upper half of the spectrum with the mirror of the lower half. The
// product of two real signals' spectra is Hermitian, so these are exactly the
// values the full-length accumulation would have produced.
void mirrorSpectrum() {
  for (int i = kHalfBins + 1; i < kFFT; ++i) {
    g_re[i] = g_re[kFFT - i];
    g_im[i] = -g_im[kFFT - i];
  }
}

void runBlock() {
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

  const bool fading = (s.target >= 0);

  if (fading) {
    accumulateWeighted(g_hre[s.active], g_him[s.active], 1.f - s.fade);
    accumulateWeighted(g_hre[s.target], g_him[s.target], s.fade);
  } else {
    accumulateUnit(g_hre[s.active], g_him[s.active]);
  }

  mirrorSpectrum();
  fft(g_re, g_im, true);

  for (int i = 0; i < kHop; ++i)
    g_out[i] = g_re[kFFT - kHop + i];

  if (fading) {
    s.fade += (float)kHop / (float)kFadeSamples;
    if (s.fade >= 1.f) {
      s.active = s.target;
      s.target = -1;
      s.fade = 0.f;
      if (s.queued >= 0) {
        s.target = s.queued;
        s.queued = -1;
      }
    }
  }
}

void updateDamping() {
  const float fc = 1200.f * expf(2.71f * s.damp);  // 1.2 kHz .. 18 kHz
  s.damp_coef =
      1.f - expf(-2.f * kPi * clipminmaxf(200.f, fc, 0.45f * kFs) / kFs);
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

void REVFX_INIT(uint32_t platform, uint32_t api) {
  (void)platform;
  (void)api;

  for (int i = 0; i < kFFT / 2; ++i) {
    const float a = 2.f * kPi * (float)i / (float)kFFT;
    g_twre[i] = cosf(a);
    g_twim[i] = -sinf(a);
  }

  s.active = 0;
  s.target = -1;
  s.queued = -1;
  s.fade = 0.f;
  s.damp = 0.7f;
  s.wet = 0.5f;

  updateDamping();
  clearBuffers();

  // One pass at load time; nothing is built while audio is running.
  for (int p = 0; p < kPresetCount; ++p)
    buildPartitions(p);
}

void REVFX_PROCESS(float *xn, uint32_t frames) {
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

void REVFX_PARAM(uint8_t index, int32_t value) {
  switch (index) {
  case k_user_revfx_param_time: {
    const int sel = (int)clipminmaxf(
        0.f, (float)value * (1.f / 1023.f) * (float)kPresetCount,
        (float)(kPresetCount - 1));

    if (sel != s.active && sel != s.target) {
      // While a crossfade runs, remember only the latest request: the knob
      // sends a stream of values and we do not want a crossfade per value.
      if (s.target >= 0)
        s.queued = sel;
      else {
        s.target = sel;
        s.fade = 0.f;
      }
    }
    break;
  }

  case k_user_revfx_param_depth:
    s.damp = clipminmaxf(0.f, (float)value * (1.f / 1023.f), 1.f);
    updateDamping();
    break;

  case k_user_revfx_param_shift_depth:
    s.wet = clipminmaxf(0.f, (float)value * (1.f / 1023.f), 1.f);
    break;

  default:
    break;
  }
}

void REVFX_SUSPEND(void) {
  clearBuffers();
  s.target = -1;
  s.queued = -1;
  s.fade = 0.f;
}

void REVFX_RESUME(void) {}
