/*
 * reso.cpp -- modal resonator for the KORG Nu:Tekt NTS-1.
 *
 * BSD 3-Clause License, see LICENSE.
 *
 * A bank of twelve tuned two-pole resonators, in the spirit of the modal mode
 * of Mutable Instruments Rings: whatever arrives at the MOD slot excites the
 * modes and the bank rings back like a struck bar, bell or string body. Feed it
 * short, noisy or plucked material and it turns into a tonal instrument; feed
 * it sustained sound and it becomes a tuned drone.
 *
 * The modes are paired with a small detune so they beat against each other,
 * which is what keeps a static bank from sounding like a simple comb filter.
 *
 * A-knob (TIME)   fundamental, 40 Hz .. 2 kHz
 * B-knob (DEPTH)  ringing time, 0.1 s .. 5 s
 */

#include "usermodfx.h"
#include "../../common/program_start.h"

namespace {

constexpr int kModes = 12;
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

// exp(-a) for the small exponents used by the resonator decay.
inline float expNegSmall(float a) {
  return 1.f - a + a * a * (0.5f - a * 0.16666667f);
}

// Stiff bar / bell like mode ratios, in near pairs so the modes beat.
const float kRatio[kModes] = {1.000f, 1.004f, 2.756f, 2.772f, 5.404f, 5.452f,
                              8.933f, 9.038f, 13.34f, 13.53f, 18.64f, 19.02f};

struct Channel {
  float y1[kModes];
  float y2[kModes];
  float hp_x;
  float hp_y;
};

struct State {
  float pitch;  // 0..1, what the bank is tuned to right now
  float decay;  // 0..1
  // What the knobs are asking for. A turn ramps towards these at a fixed rate,
  // so the twelve modes are retuned over a few milliseconds instead of being
  // replaced between one knob message and the next, which is a zipper you can
  // hear as soon as the sound is sustained.
  float pitch_target;
  float decay_target;
  float out_gain;
  float b0[kModes];
  float a1[kModes];
  float a2[kModes];
  Channel ch[2];
  int active_modes;  // modes below Nyquist, the rest is silent
};

State s;

void clearChannel(Channel &c) {
  for (int i = 0; i < kModes; ++i) {
    c.y1[i] = 0.f;
    c.y2[i] = 0.f;
  }
  c.hp_x = 0.f;
  c.hp_y = 0.f;
}

void updateCoefficients() {
  const float fs = kFs;
  const float f0 = 40.f * pow2f(5.644f * s.pitch);    // 40 Hz .. 2 kHz
  const float tau = 0.10f * pow2f(5.644f * s.decay);  // 0.1 s .. 5 s

  // A long ring only collects a fraction of the input energy, so the output
  // gain follows the decay time to keep the effect usable over the whole range.
  s.out_gain = 6.f * (1.f + 4.f * s.decay * s.decay);

  int active = 0;

  for (int n = 0; n < kModes; ++n) {
    const float f = f0 * kRatio[n];

    if (f >= 0.45f * fs) {
      s.b0[n] = 0.f;
      s.a1[n] = 0.f;
      s.a2[n] = 0.f;
      continue;
    }

    // The ratios only go up, so the modes from here on are the ones to run:
    // at a high fundamental that is often half the bank or less.
    active = n + 1;

    const float tn = tau * pow2f(-0.2885f * (float)n);  // high modes die first
    const float r = expNegSmall(1.f / (tn * fs));

    s.a1[n] = 2.f * r * fx_cosf(clipminmaxf(0.f, f / fs, 0.99f));
    s.a2[n] = -r * r;
    s.b0[n] = 0.5f * (1.f - r * r) / (1.f + 0.6f * (float)n);
  }

  s.active_modes = active;
}

// A bank that has rung down to the last floats would keep the FPU on its slow
// denormal path; at -300 dB there is nothing left to keep. True when the bank
// was down to nothing, which with a silent input means the channel is idle.
bool settle(Channel &c) {
  float mag = 0.f;
  for (int n = 0; n < kModes; ++n)
    mag += fabsf(c.y1[n]) + fabsf(c.y2[n]);
  // -180 dB: inaudible, and low enough that an idle channel can be skipped
  // rather than merely cleared.
  if (mag < 1e-9f) {
    clearChannel(c);
    return true;
  }
  return false;
}

// Digital silence in and an empty bank: every term of the sum is zero, so the
// output is zero and the work can be skipped rather than approximated.
bool idleChannel(const float *xn, uint32_t frames, Channel &c) {
  if (!settle(c))
    return false;
  for (uint32_t i = 0; i < frames; ++i)
    if (xn[i] != 0.f)
      return false;
  return true;
}

void processChannel(const float *xn, float *yn, uint32_t frames, Channel &c) {
  const int modes = s.active_modes;
  const float *const b0 = s.b0;
  const float *const a1 = s.a1;
  const float *const a2 = s.a2;
  const float out_gain = s.out_gain;

  settle(c);

  for (uint32_t i = 0; i < frames; ++i) {
    const float raw = xn[i];

    // Keep low frequency rumble out of the bank, it only muddies it up.
    c.hp_y = 0.995f * (c.hp_y + raw - c.hp_x);
    c.hp_x = raw;

    const float exc = c.hp_y;
    float acc = 0.f;

    for (int n = 0; n < modes; ++n) {
      const float y = b0[n] * exc + a1[n] * c.y1[n] + a2[n] * c.y2[n];
      c.y2[n] = c.y1[n];
      c.y1[n] = y;
      acc += y;
    }

    // A big jump of the knobs moves the poles under the state that is already
    // ringing, and a high bank can then grow by a factor of a few per sample:
    // from silence to infinity in well under a millisecond. Legitimate audio
    // never gets near this, so when it happens the bank is dropped instead of
    // being allowed to put a NaN into the slot. The input re-excites it on the
    // very next sample.
    if (!(acc > -kBankLimit && acc < kBankLimit)) {
      clearChannel(c);
      acc = 0.f;
    }

    yn[i] = fx_softclipf(0.15f, acc * out_gain);
  }
}

// Both timbres in one pass: the modal coefficients are shared by the two
// channels, so they are loaded once per mode instead of once per channel. Same
// three terms per channel in the same order, so the output does not move.
void processBoth(const float *x0, float *y0, const float *x1, float *y1,
                 uint32_t frames) {
  Channel &c0 = s.ch[0];
  Channel &c1 = s.ch[1];
  const int modes = s.active_modes;
  const float *const b0 = s.b0;
  const float *const a1 = s.a1;
  const float *const a2 = s.a2;
  const float out_gain = s.out_gain;

  for (uint32_t i = 0; i < frames; ++i) {
    const float r0 = x0[i];
    const float r1 = x1[i];

    c0.hp_y = 0.995f * (c0.hp_y + r0 - c0.hp_x);
    c0.hp_x = r0;
    c1.hp_y = 0.995f * (c1.hp_y + r1 - c1.hp_x);
    c1.hp_x = r1;

    const float e0 = c0.hp_y;
    const float e1 = c1.hp_y;
    float acc0 = 0.f;
    float acc1 = 0.f;

    for (int n = 0; n < modes; ++n) {
      const float b = b0[n];
      const float p = a1[n];
      const float q = a2[n];
      const float v0 = b * e0 + p * c0.y1[n] + q * c0.y2[n];
      const float v1 = b * e1 + p * c1.y1[n] + q * c1.y2[n];
      c0.y2[n] = c0.y1[n];
      c0.y1[n] = v0;
      acc0 += v0;
      c1.y2[n] = c1.y1[n];
      c1.y1[n] = v1;
      acc1 += v1;
    }

    if (!(acc0 > -kBankLimit && acc0 < kBankLimit)) {
      clearChannel(c0);
      acc0 = 0.f;
    }
    if (!(acc1 > -kBankLimit && acc1 < kBankLimit)) {
      clearChannel(c1);
      acc1 = 0.f;
    }

    y0[i] = fx_softclipf(0.15f, acc0 * out_gain);
    y1[i] = fx_softclipf(0.15f, acc1 * out_gain);
  }
}

}  // namespace

/*---------------------------------------------------------------------------*
 * Unit hooks
 *---------------------------------------------------------------------------*/

void MODFX_INIT(uint32_t platform, uint32_t api) {
  program_start::reset();
  (void)platform;
  (void)api;

  s.pitch = 0.35f;
  s.decay = 0.55f;
  s.pitch_target = s.pitch;
  s.decay_target = s.decay;

  updateCoefficients();
  clearChannel(s.ch[0]);
  clearChannel(s.ch[1]);
}

void MODFX_PROCESS(const float *main_xn, float *main_yn, const float *sub_xn,
                   float *sub_yn, uint32_t frames) {
  const bool has_sub = sub_yn != main_yn && sub_xn != nullptr;

  // The knobs: a straight line to the target over 20 ms, the same time whatever
  // the block size, so a fast turn and a slow one retune alike. Only recomputed
  // when something actually moved.
  {
    const float step = (float)frames * (1.f / (0.020f * kFs));
    bool moved = false;
    const float dp = s.pitch_target - s.pitch;
    if (dp != 0.f) {
      s.pitch = (fabsf(dp) <= step) ? s.pitch_target : s.pitch + (dp > 0.f ? step : -step);
      moved = true;
    }
    const float dd = s.decay_target - s.decay;
    if (dd != 0.f) {
      s.decay = (fabsf(dd) <= step) ? s.decay_target : s.decay + (dd > 0.f ? step : -step);
      moved = true;
    }
    if (moved)
      updateCoefficients();
  }

  // A channel handed digital silence with an empty bank has nothing to compute:
  // every term of its sum is zero. The instrument's second timbre is silent most
  // of the time, so this is one bank's worth of work instead of two.
  const bool main_idle = idleChannel(main_xn, frames, s.ch[0]);
  const bool sub_idle = has_sub ? idleChannel(sub_xn, frames, s.ch[1]) : true;

  if (main_idle)
    for (uint32_t i = 0; i < frames; ++i)
      main_yn[i] = 0.f;
  if (has_sub && sub_idle)
    for (uint32_t i = 0; i < frames; ++i)
      sub_yn[i] = 0.f;

  if (main_idle && sub_idle)
    return;

  if (!main_idle && has_sub && !sub_idle) {
    processBoth(main_xn, main_yn, sub_xn, sub_yn, frames);
    return;
  }
  if (!main_idle)
    processChannel(main_xn, main_yn, frames, s.ch[0]);
  if (has_sub && !sub_idle)
    processChannel(sub_xn, sub_yn, frames, s.ch[1]);
}

void MODFX_PARAM(uint8_t index, int32_t value) {
  // A program that has never held this effect sends zeroes for its knobs, and
  // zero is time zero or depth zero: the effect would look dead until a knob
  // moved. Until one of them is not zero, it keeps its own start values.
  if (!program_start::chosen(value))
    return;

  const float v = clipminmaxf(0.f, (float)value * (1.f / 1023.f), 1.f);

  switch (index) {
  case k_user_modfx_param_time:
    s.pitch_target = v;
    return;

  case k_user_modfx_param_depth:
    s.decay_target = v;
    return;

  default:
    return;
  }
}

void MODFX_SUSPEND(void) {
  clearChannel(s.ch[0]);
  clearChannel(s.ch[1]);
}

void MODFX_RESUME(void) {}
