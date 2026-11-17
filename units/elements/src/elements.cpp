/*
 * elements.cpp -- geometry morphing resonator for the KORG Nu:Tekt NTS-1.
 *
 * BSD 3-Clause License, see LICENSE.
 *
 * The sibling of reso, with a different idea behind it: instead of one fixed
 * set of modes, the mode ratios themselves are morphed while you play. One end
 * of the morph is a stiff string (harmonic series with the inharmonic stretch a
 * real string has), then the Bessel ratios of a circular membrane, then the
 * modes of a flat plate, and finally the inharmonic series of a bell. Decay and
 * damping follow the geometry, so a string is short and bright and a bell rings
 * long and dark, which is what those objects do. The exciter is filtered along
 * the way as well, the way a stiff mallet excites a string differently than a
 * soft one excites a plate.
 *
 * It is an independent implementation, inspired by the resonators of Mutable
 * Instruments Elements rather than a port of it: the NTS-1 gives a modulation
 * effect two knobs and no note information, so pitch is set by hand here.
 *
 * A-knob (TIME)   fundamental, 40 Hz .. 2 kHz
 * B-knob (DEPTH)  geometry: string -> membrane -> plate -> bell
 */

#include "usermodfx.h"

namespace {

constexpr int kModes = 16;
constexpr float kFs = 48000.f;

// The bank normally sums to well below one; anything past this is a runaway.
constexpr float kBankLimit = 100.f;

// Small, libm free helpers: the 6 KB SRAM budget of a modfx unit does not
// leave room for the full maths library.
inline float pow2f(float x) {
  int i = (int)x;
  if (x < 0.f && (float)i != x)
    --i;
  float r = fx_pow2f(clipminmaxf(0.f, x - (float)i, 2.5f));
  for (; i > 0; --i)
    r *= 2.f;
  for (; i < 0; ++i)
    r *= 0.5f;
  return r;
}

inline float expNegSmall(float a) {
  return 1.f - a + a * a * (0.5f - a * 0.16666667f);
}

// Circular membrane: ratios of the Bessel zeros, 1.000 being the fundamental.
static const float kMembrane[kModes] = {
    1.000f, 1.593f, 2.135f, 2.295f, 2.653f, 2.917f, 3.155f, 3.500f,
    3.598f, 3.647f, 4.059f, 4.230f, 4.831f, 5.132f, 5.428f, 5.467f};

// Flat plate: two dimensional bending modes, dense and fairly even.
static const float kPlate[kModes] = {
    1.000f, 1.580f, 2.000f, 2.280f, 2.650f, 3.160f, 3.500f, 4.000f,
    4.470f, 5.000f, 5.480f, 6.000f, 6.480f, 7.000f, 7.500f, 8.000f};

// Bell: strongly inharmonic, the high partials carry a lot of the character.
static const float kBell[kModes] = {
    1.000f, 2.000f, 2.400f, 3.000f, 3.900f, 4.600f, 5.400f, 6.800f,
    7.900f, 9.200f, 10.60f, 12.10f, 13.70f, 15.40f, 17.20f, 19.10f};

struct Channel {
  float y1[kModes];
  float y2[kModes];
  float hp_x;
  float hp_y;
  float lp;
};

struct State {
  float pitch;     // 0..1
  float geometry;  // 0..1
  float ratio[4][kModes];
  float b0[kModes];
  float a1[kModes];
  float a2[kModes];
  float exc_coef;  // exciter low pass
  float out_gain;
  Channel ch[2];
  int active_modes;  // modes below Nyquist, the rest is silent
};

State s;

void buildRatioTable() {
  for (int k = 0; k < kModes; ++k) {
    const float n = (float)(k + 1);
    // A real string is stiff: f_n = n * f0 * sqrt(1 + B n^2), which stretches
    // the partials apart as they go up.
    s.ratio[0][k] = n * sqrtf(1.f + 0.0008f * n * n);
    s.ratio[1][k] = kMembrane[k];
    s.ratio[2][k] = kPlate[k];
    s.ratio[3][k] = kBell[k];
  }
}

void clearChannel(Channel &c) {
  for (int i = 0; i < kModes; ++i) {
    c.y1[i] = 0.f;
    c.y2[i] = 0.f;
  }
  c.hp_x = 0.f;
  c.hp_y = 0.f;
  c.lp = 0.f;
}

void updateCoefficients() {
  const float g = s.geometry * 3.f;  // 0 string .. 3 bell
  int i0 = (int)g;
  if (i0 > 2)
    i0 = 2;
  const float t = g - (float)i0;

  const float f0 = 40.f * pow2f(5.644f * s.pitch);   // 40 Hz .. 2 kHz
  // Decay time constant of the fundamental. The range is wide on purpose: the
  // string end damps away in a third of a second, the bell end rings for
  // minutes, and damping shortens the higher modes on top of that. The very
  // top of the range is a sustain rather than a bell, which is what makes the
  // plate and bell end usable as a pad when you hold a note.
  const float tau = 0.35f * pow2f(3.f * g);
  const float damp = 0.30f - 0.05f * g;              // harder geometries ring longer

  int active = 0;

  for (int k = 0; k < kModes; ++k) {
    const float ratio =
        s.ratio[i0][k] + (s.ratio[i0 + 1][k] - s.ratio[i0][k]) * t;
    const float f = f0 * ratio;

    if (f >= 0.45f * kFs) {
      s.b0[k] = 0.f;
      s.a1[k] = 0.f;
      s.a2[k] = 0.f;
      continue;
    }

    // Every mode set is sorted by frequency, so the ones past this point are
    // all above Nyquist: the sample loop can stop here instead of running the
    // tail of the bank for nothing. It matters most at the top of the range,
    // where a bell keeps two or three modes.
    active = k + 1;

    const float tn = tau * pow2f(-damp * (float)k);
    const float r = expNegSmall(1.f / (tn * kFs));

    s.a1[k] = 2.f * r * fx_cosf(clipminmaxf(0.f, f / kFs, 0.99f));
    s.a2[k] = -r * r;
    s.b0[k] = 0.5f * (1.f - r * r) / (1.f + 0.5f * (float)k);
  }

  s.active_modes = active;

  // A stiff mallet on a string is brighter than a soft one on a plate.
  const float exc_fc = 700.f * pow2f(1.6f * (3.f - g));
  s.exc_coef = 1.f - expNegSmall(6.2831853f * clipminmaxf(200.f, exc_fc, 12000.f) / kFs);
  s.out_gain = 6.f * (1.f + 2.f * g);
}

void processChannel(const float *xn, float *yn, uint32_t frames, Channel &c) {
  const int modes = s.active_modes;
  const float *const b0 = s.b0;
  const float *const a1 = s.a1;
  const float *const a2 = s.a2;
  const float exc_coef = s.exc_coef;
  const float out_gain = s.out_gain;

  for (uint32_t i = 0; i < frames; ++i) {
    const float raw = xn[i];

    // Remove rumble, then shape the exciter.
    c.hp_y = 0.995f * (c.hp_y + raw - c.hp_x);
    c.hp_x = raw;
    c.lp += (c.hp_y - c.lp) * exc_coef;

    const float exc = c.lp;
    float acc = 0.f;

    for (int n = 0; n < modes; ++n) {
      const float y = b0[n] * exc + a1[n] * c.y1[n] + a2[n] * c.y2[n];
      c.y2[n] = c.y1[n];
      c.y1[n] = y;
      acc += y;
    }

    // Moving the geometry or the pitch jumps the modes under the ringing
    // state, and a high bank can then grow by a factor of a few per sample.
    // Legitimate audio never gets near this limit, so a runaway bank is simply
    // dropped rather than allowed to put a NaN into the slot.
    if (!(acc > -kBankLimit && acc < kBankLimit)) {
      clearChannel(c);
      acc = 0.f;
    }

    yn[i] = fx_softclipf(0.15f, acc * out_gain);
  }
}

}  // namespace

/*---------------------------------------------------------------------------*
 * Unit hooks
 *---------------------------------------------------------------------------*/

void MODFX_INIT(uint32_t platform, uint32_t api) {
  (void)platform;
  (void)api;

  s.pitch = 0.45f;
  s.geometry = 0.35f;

  buildRatioTable();
  updateCoefficients();
  clearChannel(s.ch[0]);
  clearChannel(s.ch[1]);
}

void MODFX_PROCESS(const float *main_xn, float *main_yn, const float *sub_xn,
                   float *sub_yn, uint32_t frames) {
  processChannel(main_xn, main_yn, frames, s.ch[0]);

  if (sub_yn != main_yn && sub_xn != nullptr)
    processChannel(sub_xn, sub_yn, frames, s.ch[1]);
}

void MODFX_PARAM(uint8_t index, int32_t value) {
  const float v = clipminmaxf(0.f, (float)value * (1.f / 1023.f), 1.f);

  switch (index) {
  case k_user_modfx_param_time:
    s.pitch = v;
    break;

  case k_user_modfx_param_depth:
    s.geometry = v;
    break;

  default:
    return;
  }

  updateCoefficients();
}

void MODFX_SUSPEND(void) {
  clearChannel(s.ch[0]);
  clearChannel(s.ch[1]);
}

void MODFX_RESUME(void) {}
