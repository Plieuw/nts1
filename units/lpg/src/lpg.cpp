/*
 * lpg.cpp -- vactrol style low pass gate for the KORG Nu:Tekt NTS-1.
 *
 * BSD 3-Clause License, see LICENSE.
 *
 * The NTS-1 has no user slot for an envelope generator, so this unit listens to
 * the amplitude of whatever arrives at the MOD slot and lets that stand in for
 * an envelope: loud passages open both a VCA and a low pass filter, quiet ones
 * close them again. The release time is deliberately slow and non-linear, which
 * is what gives a vactrol gate its plucked, breathing character.
 *
 * A-knob (TIME)   release time of the gate, 4 ms .. 2 s
 * B-knob (DEPTH)  how far the gate closes: 0 is nearly open, 1 is a full gate
 *
 * The follower is normalised with a slow peak tracker, so the gate opens the
 * same way for quiet and loud sources.
 */

#include "usermodfx.h"

namespace {

constexpr float kFs = 48000.f;
constexpr int kCoefUpdate = 32;

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

// exp(-a) for the small exponents used by the time constants.
inline float expNegSmall(float a) {
  return 1.f - a + a * a * (0.5f - a * 0.16666667f);
}  // samples between filter coefficient updates

struct Channel {
  float env;       // envelope follower
  float peak;      // slow peak tracker used to normalise the follower
  float inv_peak;  // 1/peak, refreshed with the filter coefficients
  float ic1;       // SVF integrator states
  float ic2;
  float a1;
  float a2;
  float a3;
};

struct State {
  float time;   // 0..1
  float depth;  // 0..1
  float atk_coef;
  float rel_coef;
  Channel ch[2];
  int coef_counter;
};

State s;

void clearChannel(Channel &c) {
  c.env = 0.f;
  c.peak = 1e-4f;
  c.inv_peak = 1e4f;
  c.ic1 = 0.f;
  c.ic2 = 0.f;
  c.a1 = 1.f;
  c.a2 = 0.f;
  c.a3 = 0.f;
}

void updateCoefficients() {
  const float tau = 0.004f * pow2f(8.945f * s.time);  // 4 ms .. 2 s
  s.rel_coef = expNegSmall(1.f / (tau * kFs));
  s.atk_coef = expNegSmall(1.f / (0.0012f * kFs));
}

// Openness (0..1) of one channel: the follower normalised by the peak tracker,
// smoothed by the LDR curve and scaled by the depth knob. Used for the gain and
// for the cutoff alike, so the knob is applied once and not once per path.
inline float openness(const Channel &c) {
  float e = c.env * c.inv_peak;
  e = clipminmaxf(0.f, e, 1.f);
  e = e * e * (3.f - 2.f * e);  // smoothstep, the non-linearity of the LDR
  return (1.f - s.depth) + s.depth * e;
}

void updateFilter(Channel &c) {
  // The reciprocal is taken here rather than per sample, and the tracker only
  // moves by 0.99998 per sample, so it is never more than one update stale.
  c.inv_peak = 1.f / c.peak;
  const float fc = clipminmaxf(60.f, 180.f * pow2f(5.626f * openness(c)), 16000.f);
  const float g = fx_tanpif(clipminmaxf(0.0001f, fc / kFs, 0.45f));
  const float k = 0.9f;  // Q ~ 1.1, a touch of resonance keeps it vocal
  const float a1 = 1.f / (1.f + g * (g + k));
  c.a1 = a1;
  c.a2 = g * a1;
  c.a3 = g * c.a2;
}

void processChannel(const float *xn, float *yn, uint32_t frames, Channel &c) {
  for (uint32_t i = 0; i < frames; ++i) {
    if (--s.coef_counter <= 0) {
      updateFilter(c);
      if (s.coef_counter <= 0)
        s.coef_counter = kCoefUpdate;
    }

    const float in = xn[i];
    const float rect = fabsf(in);

    if (rect > c.env)
      c.env = s.atk_coef * c.env + (1.f - s.atk_coef) * rect;
    else
      c.env = s.rel_coef * c.env + (1.f - s.rel_coef) * rect;

    // ~1 s release, only used to make the gate level independent.
    c.peak *= 0.99998f;
    if (rect > c.peak)
      c.peak = rect;
    if (c.peak < 1e-3f)
      c.peak = 1e-3f;

    // The depth is already part of the openness.
    const float gain = openness(c);

    const float v3 = in - c.ic2;
    const float v1 = c.a1 * c.ic1 + c.a2 * v3;
    const float v2 = c.ic2 + c.a2 * c.ic1 + c.a3 * v3;
    c.ic1 = 2.f * v1 - c.ic1;
    c.ic2 = 2.f * v2 - c.ic2;

    yn[i] = fx_softclipf(0.1f, v2 * gain * 1.2f);
  }

  // Between notes the follower and the filter fall through the last floats of
  // the range, and the M4's FPU runs its denormal path for every one of them.
  // At -300 dB there is nothing to keep, so the state is zeroed instead.
  if (c.env < 1e-15f && fabsf(c.ic1) < 1e-15f && fabsf(c.ic2) < 1e-15f) {
    c.env = 0.f;
    c.ic1 = 0.f;
    c.ic2 = 0.f;
  }
}

}  // namespace

/*---------------------------------------------------------------------------*
 * Unit hooks
 *---------------------------------------------------------------------------*/

void MODFX_INIT(uint32_t platform, uint32_t api) {
  (void)platform;
  (void)api;

  s.time = 0.55f;
  s.depth = 0.75f;
  s.coef_counter = 1;

  updateCoefficients();
  clearChannel(s.ch[0]);
  clearChannel(s.ch[1]);
}

void MODFX_PROCESS(const float *main_xn, float *main_yn, const float *sub_xn,
                   float *sub_yn, uint32_t frames) {
  processChannel(main_xn, main_yn, frames, s.ch[0]);

  // On the NTS-1 there is a single timbre; only touch the sub buffers when the
  // platform actually hands us a second one.
  if (sub_yn != main_yn && sub_xn != nullptr) {
    s.coef_counter = 1;
    processChannel(sub_xn, sub_yn, frames, s.ch[1]);
  }
}

void MODFX_PARAM(uint8_t index, int32_t value) {
  const float v = clipminmaxf(0.f, (float)value * (1.f / 1023.f), 1.f);

  switch (index) {
  case k_user_modfx_param_time:
    s.time = v;
    updateCoefficients();
    break;

  case k_user_modfx_param_depth:
    s.depth = v;
    break;

  default:
    break;
  }
}

void MODFX_SUSPEND(void) {
  clearChannel(s.ch[0]);
  clearChannel(s.ch[1]);
}

void MODFX_RESUME(void) {
  s.coef_counter = 1;
}
