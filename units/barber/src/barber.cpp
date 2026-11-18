/*
 * barber.cpp -- barberpole phaser for the KORG Nu:Tekt NTS-1.
 *
 * BSD 3-Clause License, see LICENSE.
 *
 * A sweeps through a cascade of one to sixteen all-pass sections, half of them
 * landing as a notch in the audible range, and where a single sweep would run
 * out of spectrum and jump back, a second cascade a half cycle out of phase
 * takes over. The two cascades are crossfaded with a raised cosine so the jump
 * is never heard, which gives the endless rise (or fall) of a barberpole.
 * Feedback sharpens the notches into the classic jet sound.
 *
 * A-knob (TIME)   rate and direction: left of centre falls, right of centre
 *                 rises, further from centre is faster (0.02 .. 10 Hz)
 * B-knob (DEPTH)  feedback / resonance of the phaser
 * DEL + B knob    number of notches, one to sixteen
 *
 * The dry/wet mix sits at the middle, where the notches are deepest: with the
 * whole cascade on the wet side they are a phase sweep and nothing else, and at
 * zero there is no phaser at all. The notch count was the thing missing from the
 * three controls this slot has, so it took the shifted DEPTH control and the mix
 * became the fixed one.
 */

#include "userdelfx.h"
#include "../../common/program_start.h"
#include "../../common/silence.h"

namespace {

constexpr int kStages = 16;
constexpr float kFs = 48000.f;
// Each cascade is mixed with the dry signal at the middle, which is where its
// notches are deepest. The two are then crossfaded and that sum is the output:
// mixing it with the dry signal a second time put a 6 dB ceiling on the notches
// (a quarter of the all-pass against three quarters of the dry) and that ceiling
// was most of why the sweep sounded like a phase drift instead of a barberpole.
constexpr float kHalf = 0.5f;
// The corner the sweep travels: two decades and a bit, which puts the highest
// notch of the sixteen at 3.7 times the corner and the lowest at a tenth of it,
// so every notch crosses seven octaves on its way up.
constexpr float kSweepLo = 40.f;
constexpr float kSweepOct = 7.32f;
// The 50/50 notch mix is 2.5 dB down on the dry signal on average, and an effect
// that drops the level is a level change, not a phaser: this brings it back to
// unity at the middle of the knobs, where the sweep spends its time.
constexpr float kMakeup = 0.80f;
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
  int stages;   // notches, 1..kStages
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

// Small, libm free helpers. One libm call is enough to pull newlib's reentrancy
// struct (about a kilobyte of RAM) and its errno machinery into the unit, and
// the sweep only needs a couple of table lookups.
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

inline float expf_fast(float x) { return pow2f(x * 1.44269504f); }

// The first-order allpass below is y = x1 + c (x - y1), whose pole sits at -c:
// the corner is at fc only when c is negative and close to -1, which is what
// (t - 1)/(t + 1) gives. The mirrored form (1 - t)/(1 + t) puts the pole at -1
// instead, so its corner lands at fs/2 - fc: every notch of the cascade sat in
// the top octave, and the sweep had almost nothing to move in the band. That is
// what the unit was doing until 1.5-0, and it is why it read as a phase drift.
inline float allpassCoef(float fc) {
  const float t = fx_tanpif(clipminmaxf(0.0001f, fc / kFs, 0.45f));
  return (t - 1.f) / (t + 1.f);
}

void updateRate() {
  // A centre detent stops the sweep, distance from it sets the speed.
  const float off = 2.f * s.time - 1.f;
  s.dir = (off < 0.f) ? -1.f : 1.f;
  const float speed = fabsf(off);
  s.rate = s.dir * 0.02f * expf_fast(6.21f * speed);  // 0.02 .. 10 Hz
  // Enough feedback that the notches ring into the jet the barberpole is known
  // for; the soft clip in the loop keeps the resonance bounded.
  s.fb_gain = 0.8f * s.depth;
}

void updateSweep() {
  // Two exponentials a half cycle apart, 40 Hz .. 6.4 kHz.
  const float fa = kSweepLo * expf_fast(kSweepOct * s.phase);
  float fb_phase = s.phase + 0.5f;
  if (fb_phase >= 1.f)
    fb_phase -= 1.f;
  const float fb = kSweepLo * expf_fast(kSweepOct * fb_phase);

  s.ca = allpassCoef(clipminmaxf(20.f, fa, 0.45f * kFs));
  s.cb = allpassCoef(clipminmaxf(20.f, fb, 0.45f * kFs));

  const float w = 0.5f - 0.5f * fx_cosf(s.phase);
  s.wa = w;
  s.wb = 1.f - w;
}

}  // namespace

/*---------------------------------------------------------------------------*
 * Unit hooks
 *---------------------------------------------------------------------------*/

void DELFX_INIT(uint32_t platform, uint32_t api) {
  program_start::reset();
  (void)platform;
  (void)api;

  s.time = 0.75f;
  s.depth = 0.5f;
  s.stages = kStages;
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
  const int nstages = s.stages;

  // A cascade that has rung down to the last floats would keep the FPU on its
  // slow denormal path for every stage; below -180 dB there is nothing to keep.
  bool down[2];
  for (int ch = 0; ch < 2; ++ch) {
    Channel &c = s.ch[ch];
    float mag = fabsf(c.fb);
    for (int i = 0; i < kStages; ++i)
      mag += fabsf(c.a[i].y1) + fabsf(c.b[i].y1);
    down[ch] = mag < 1e-9f;
    if (down[ch])
      clearChannel(c);
  }

  // Digital silence in and both cascades down to nothing: the output is zero
  // either way. The delay slot is handed silence whenever nothing is playing,
  // which is most of the time.
  if (down[0] && down[1] && silence::block(xn, frames, 2)) {
    for (uint32_t i = 0; i < frames * 2u; ++i)
      xn[i] = 0.f;
    return;
  }

  for (int i = 0; i < nframes; ++i) {
    for (int ch = 0; ch < 2; ++ch) {
      Channel &c = s.ch[ch];
      const float in = xn[2 * i + ch];

      const float exc = fx_softclipf(0.2f, in + c.fb * s.fb_gain);

      // The two cascades are independent of each other, so they are stepped in
      // one pass over the stages: the same arithmetic per stage, but two chains
      // for the core to interleave instead of one at a time, and half the loop
      // bookkeeping.
      float xa = exc;
      float xb = exc;
      for (int st = 0; st < nstages; ++st) {
        Stage &ga = c.a[st];
        Stage &gb = c.b[st];
        const float ya = ga.x1 + s.ca * (xa - ga.y1);
        const float yb = gb.x1 + s.cb * (xb - gb.y1);
        ga.x1 = xa;
        ga.y1 = ya;
        gb.x1 = xb;
        gb.y1 = yb;
        xa = ya;
        xb = yb;
      }

      // Each cascade is mixed with the dry signal on its own first: that is
      // what turns an all-pass sweep into audible notches. Crossfading the
      // cascades before the mix would only average their phase away.
      const float na = in + (xa - in) * kHalf;
      const float nb = in + (xb - in) * kHalf;
      const float wet = s.wa * na + s.wb * nb;

      c.fb = fx_softclipf(0.2f, wet);

      xn[2 * i + ch] = kMakeup * wet;
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
  // A program that has never held this effect sends zeroes for its knobs, and
  // zero is time zero or depth zero: the effect would look dead until a knob
  // moved. Until one of them is not zero, it keeps its own start values.
  if (!program_start::chosen(value))
    return;

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

  case k_user_delfx_param_shift_depth: {
    // One to eight notches, in whole numbers: half a notch is not a thing.
    const int n = 1 + (int)(v * (float)(kStages - 1) + 0.5f);
    if (n == s.stages)
      break;
    s.stages = n;
    // A stage that comes back into the sweep starts empty instead of replaying
    // whatever it held the last time it was on, which would be a click from a
    // filter state that no longer belongs to this sweep.
    for (int ch = 0; ch < 2; ++ch)
      for (int st = n; st < kStages; ++st) {
        s.ch[ch].a[st].x1 = 0.f;
        s.ch[ch].a[st].y1 = 0.f;
        s.ch[ch].b[st].x1 = 0.f;
        s.ch[ch].b[st].y1 = 0.f;
      }
    break;
  }

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
