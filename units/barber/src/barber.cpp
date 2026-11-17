/*
 * barber.cpp -- barberpole phaser for the KORG Nu:Tekt NTS-1.
 *
 * BSD 3-Clause License, see LICENSE.
 *
 * A sweeps through a cascade of eight all-pass sections, and where a single
 * sweep would run out of spectrum and jump back, a second cascade a half cycle
 * out of phase takes over. The two cascades are crossfaded with a raised cosine
 * so the jump is never heard, which gives the endless rise (or fall) of a
 * barberpole. Feedback sharpens the notches into the classic jet sound.
 *
 * A-knob (TIME)   rate and direction: left of centre falls, right of centre
 *                 rises, further from centre is faster (0.02 .. 10 Hz)
 * B-knob (DEPTH)  feedback / resonance of the phaser
 * DEL + B knob    dry/wet mix
 */

#include "userdelfx.h"

namespace {

constexpr int kStages = 8;
constexpr float kFs = 48000.f;
constexpr float kPi = 3.14159265358979f;

struct Stage {
  float x1;
  float y1;
};

struct Channel {
  Stage a[kStages];
  Stage b[kStages];
  float fb;
};

struct State {
  float time;   // 0..1
  float depth;  // 0..1
  float wet;    // 0..1
  float rate;   // Hz
  float dir;    // -1 or +1
  float phase;  // 0..1
  float ca;     // all-pass coefficient of the rising cascade
  float cb;     // all-pass coefficient of the falling cascade
  float wa;     // crossfade weights
  float wb;
  float fb_gain;
  int   coef_tick;
  Channel ch[2];
};

State s;

void clearChannel(Channel &c) {
  for (int i = 0; i < kStages; ++i) {
    c.a[i].x1 = 0.f;
    c.a[i].y1 = 0.f;
    c.b[i].x1 = 0.f;
    c.b[i].y1 = 0.f;
  }
  c.fb = 0.f;
}

inline float allpassCoef(float fc) {
  const float t = tanf(kPi * fc / kFs);
  return (1.f - t) / (1.f + t);
}

void updateRate() {
  // A centre detent stops the sweep, distance from it sets the speed.
  const float off = 2.f * s.time - 1.f;
  s.dir = (off < 0.f) ? -1.f : 1.f;
  const float speed = fabsf(off);
  s.rate = s.dir * 0.02f * expf(6.21f * speed);  // 0.02 .. 10 Hz
  s.fb_gain = 0.65f * s.depth;
}

void updateSweep() {
  // Two exponentials a half cycle apart, 60 Hz .. 9.6 kHz.
  const float fa = 60.f * expf(5.07f * s.phase);
  float fb_phase = s.phase + 0.5f;
  if (fb_phase >= 1.f)
    fb_phase -= 1.f;
  const float fb = 60.f * expf(5.07f * fb_phase);

  s.ca = allpassCoef(clipminmaxf(60.f, fa, 0.45f * kFs));
  s.cb = allpassCoef(clipminmaxf(60.f, fb, 0.45f * kFs));

  const float w = 0.5f - 0.5f * cosf(2.f * kPi * s.phase);
  s.wa = w;
  s.wb = 1.f - w;
}

}  // namespace

/*---------------------------------------------------------------------------*
 * Unit hooks
 *---------------------------------------------------------------------------*/

void DELFX_INIT(uint32_t platform, uint32_t api) {
  (void)platform;
  (void)api;

  s.time = 0.75f;
  s.depth = 0.5f;
  s.wet = 0.5f;
  s.phase = 0.f;
  s.coef_tick = 1;

  updateRate();
  updateSweep();
  clearChannel(s.ch[0]);
  clearChannel(s.ch[1]);
}

void DELFX_PROCESS(float *xn, uint32_t frames) {
  const int nframes = (int)frames;
  const float inc = s.rate / kFs;

  // A cascade that has rung down to the last floats would keep the FPU on its
  // slow denormal path for every stage; at -300 dB there is nothing to keep.
  for (int ch = 0; ch < 2; ++ch) {
    Channel &c = s.ch[ch];
    float mag = fabsf(c.fb);
    for (int i = 0; i < kStages; ++i)
      mag += fabsf(c.a[i].y1) + fabsf(c.b[i].y1);
    if (mag < 1e-15f)
      clearChannel(c);
  }

  for (int i = 0; i < nframes; ++i) {
    for (int ch = 0; ch < 2; ++ch) {
      Channel &c = s.ch[ch];
      const float in = xn[2 * i + ch];

      const float exc = fx_softclipf(0.2f, in + c.fb * s.fb_gain);

      float xa = exc;
      for (int st = 0; st < kStages; ++st) {
        Stage &g = c.a[st];
        const float y = g.x1 + s.ca * (xa - g.y1);
        g.x1 = xa;
        g.y1 = y;
        xa = y;
      }

      float xb = exc;
      for (int st = 0; st < kStages; ++st) {
        Stage &g = c.b[st];
        const float y = g.x1 + s.cb * (xb - g.y1);
        g.x1 = xb;
        g.y1 = y;
        xb = y;
      }

      // The two cascades sum incoherently with the dry signal, so trim the wet
      // path to keep the effect roughly unity gain.
      // Each cascade is mixed with the dry signal on its own first: that is
      // what turns an all-pass sweep into audible notches. Crossfading the
      // cascades before the mix would only average their phase away.
      const float na = in + (xa - in) * 0.5f;
      const float nb = in + (xb - in) * 0.5f;
      const float wet = s.wa * na + s.wb * nb;

      c.fb = fx_softclipf(0.2f, wet);

      xn[2 * i + ch] = in + (wet - in) * s.wet;
    }

    s.phase += inc;
    while (s.phase >= 1.f)
      s.phase -= 1.f;
    while (s.phase < 0.f)
      s.phase += 1.f;

    if (--s.coef_tick <= 0) {
      s.coef_tick = 16;
      updateSweep();
    }
  }
}

void DELFX_PARAM(uint8_t index, int32_t value) {
  const float v = clipminmaxf(0.f, (float)value * (1.f / 1023.f), 1.f);

  switch (index) {
  case k_user_delfx_param_time:
    s.time = v;
    updateRate();
    break;

  case k_user_delfx_param_depth:
    s.depth = v;
    updateRate();
    break;

  case k_user_delfx_param_shift_depth:
    s.wet = v;
    break;

  default:
    break;
  }
}

void DELFX_SUSPEND(void) {
  clearChannel(s.ch[0]);
  clearChannel(s.ch[1]);
}

void DELFX_RESUME(void) {
  s.coef_tick = 1;
}
