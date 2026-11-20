/*
 * wavefolder.cpp -- five fold wavefolder for the KORG Nu:Tekt NTS-1.
 *
 * BSD 3-Clause License, see LICENSE.
 *
 * A folder turns a signal back on itself at full scale instead of clipping it:
 * what a clipper would have flattened into a straight line becomes another trip
 * up the curve, and the harmonics that adds are the effect. This one folds at
 * most five times, so the knob walks from a clean signal into five reflections
 * and then rides the cap, instead of turning into a clipper with a knob on it.
 *
 * Both curves a folder is usually built from are here, and the second knob is
 * the one in between: the triangle fold is the hard, buzzy end (a piecewise
 * straight line, so every fold is a corner) and the sine fold is the round one.
 *
 * A-knob (TIME)   FOLD   -- drive into the folds, 1x .. 5x
 * B-knob (DEPTH)  SHAPE  -- triangle fold .. sine fold
 */

#include "usermodfx.h"
#include "../../common/arcmath.h"
#include "../../common/program_start.h"
#include "../../common/silence.h"

namespace {

constexpr float kFs = 48000.f;

// Five folds and no more. Past the fifth reflection the shaper stays at full
// scale; without the cap, 8x of drive on full scale material is a square wave
// and the point of folding (harmonics that still follow the input) is gone.
constexpr float kMaxFolds = 5.f;

// The FOLD knob is the drive: 1x at the bottom, where a full scale signal only
// just reaches the fold point and anything below it passes untouched, up to 5x
// at the top, where a full scale signal is folded five times — the same point
// the cap sits at. The two used to be apart: with 8x at the top, a full scale
// signal was at the cap from 77% of the knob on and the last quarter did
// nothing at all. Exponential, because that is how the fold rate is heard: the
// same turn adds the same number of reflections whatever the input level is.
constexpr float kDriveSpan = 2.3219281f;  // log2(5): 1x .. 5x, one fold per step
constexpr float kDriveMin = 1.f;
constexpr float kDriveMax = 5.f;

// Drive is also gain, and a folder whose level follows its knob is a volume knob
// with extra steps. What the level does as the drive goes up is not 1/drive,
// though: a folded signal is bounded, so its RMS stops growing once the input is
// being folded, while the clean end still tracks the drive. Measured on a pluck
// that peaks at 0.9, the level of a loud patch, the curve alone is 1.7 dB up at
// the clean end, 6.4 dB at the middle of the knob and 7.3 dB at the top. So the
// gain is 1.7 dB down throughout, plus a compensation that saturates with the
// drive: 1.0 at the clean end to about 1.9 from the middle on, which is 5.6 dB.
// That leaves the sweep within 0.3 dB of the dry level, the middle at 0.0 dB.
constexpr float kTrim = 0.82f;
constexpr float kCompDepth = 0.9f;
// 2^-1.8u: the compensation is within half a decibel of its final value by the
// middle of the knob, which is where the curve has folded everything it is going
// to fold. It was 2.4 while the drive ran to 8; with the drive on the folds
// themselves, 1.8 is what tracks them.
constexpr float kCompRate = 1.8f;

// Knob moves are slopes over 20 ms, spread across the samples of the block: a
// folder is a gain into a curve, and a gain that steps between parameter
// messages is a click on anything sustained. Two steps, because the drive walks
// 1..5 and the shape only 0..1.
constexpr float kShapeStep = 1.f / (0.020f * kFs);
constexpr float kDriveStep = (kDriveMax - kDriveMin) / (0.020f * kFs);

constexpr float kHalfPi = 1.57079633f;

struct State {
  float drive;         // what the folder is being driven with right now
  float drive_target;
  float shape;         // 0 = triangle fold, 1 = sine fold
  float shape_target;
};

State s;

// One step at a time towards a target, never past it.
inline float approach(float v, float target, float step) {
  const float d = target - v;
  if (d == 0.f)
    return v;
  if (d > 0.f)
    return (d <= step) ? target : v + step;
  return (-d <= step) ? target : v - step;
}

// |u| folded, with a period of four: up to 1, back through zero at 2, down to
// -1 at 3, and back. Piecewise straight, so the corners are sharp.
inline float triangleFold(float a) {
  const float p = a - 4.f * (float)(int)(a * 0.25f);
  if (p <= 1.f)
    return p;
  if (p <= 3.f)
    return 2.f - p;
  return p - 4.f;
}

// The same walk with round corners: sin(pi/2 * |u|), which is the classic sine
// folder. The four quarter waves are picked out before the sine is evaluated so
// the polynomial in arcmath.h only ever sees an angle below pi/2, where it is
// good to a couple of parts per million.
inline float sineFold(float a) {
  const float p = a - 4.f * (float)(int)(a * 0.25f);
  float w, sign;
  if (p <= 1.f) {
    w = p;
    sign = 1.f;
  } else if (p <= 2.f) {
    w = 2.f - p;
    sign = 1.f;
  } else if (p <= 3.f) {
    w = p - 2.f;
    sign = -1.f;
  } else {
    w = 4.f - p;
    sign = -1.f;
  }
  float sn, cs;
  arc_sincos(w * kHalfPi, sn, cs);
  return sign * sn;
}

// The folder itself. Both curves are odd, so the sign is taken off and put back
// rather than evaluated twice.
inline float fold(float x, float drive, float shape) {
  const float u = x * drive;
  float a = fabsf(u);
  if (a > kMaxFolds)
    a = kMaxFolds;
  const float tri = triangleFold(a);
  const float y = (shape <= 0.f) ? tri : tri + shape * (sineFold(a) - tri);
  return (u < 0.f) ? -y : y;
}

}  // namespace

/*---------------------------------------------------------------------------*
 * Unit hooks
 *---------------------------------------------------------------------------*/

void MODFX_INIT(uint32_t platform, uint32_t api) {
  program_start::reset();
  (void)platform;
  (void)api;

  s.drive = 2.2360680f;  // sqrt(5): the middle of the knob
  s.drive_target = s.drive;
  s.shape = 0.5f;  // between the two curves
  s.shape_target = s.shape;
}

void MODFX_PROCESS(const float *main_xn, float *main_yn, const float *sub_xn,
                   float *sub_yn, uint32_t frames) {
  const bool has_sub = sub_yn != main_yn && sub_xn != nullptr;

  // The ramps, a straight line over 20 ms whatever the block size, spread over
  // the samples of this block.
  const float drive_from = s.drive;
  const float shape_from = s.shape;
  const float drive_to = approach(s.drive, s.drive_target, (float)frames * kDriveStep);
  const float shape_to = approach(s.shape, s.shape_target, (float)frames * kShapeStep);
  s.drive = drive_to;
  s.shape = shape_to;
  // The compensation follows the ramped drive, so the two move together:
  // 1 + 0.9 * (1 - 2^(-2.4 (drive - 1))), which is 1 at the clean end and 1.9
  // from the middle of the knob on. Two of these a block, none a sample.
  const float comp_from =
      1.f + kCompDepth * (1.f - arc_pow2f(-kCompRate * (drive_from - kDriveMin)));
  const float comp_to =
      1.f + kCompDepth * (1.f - arc_pow2f(-kCompRate * (drive_to - kDriveMin)));
  const float gain_from = kTrim / comp_from;
  const float gain_to = kTrim / comp_to;

  // Digital silence in, and the folder has no state at all: fold(0) is exactly
  // zero, so the block is a zero block and the curve does not have to be
  // evaluated. The ramps above still ran, so a knob moved over silence lands
  // where it should.
  const bool main_silent = silence::block(main_xn, frames);
  const bool sub_silent = !has_sub || silence::block(sub_xn, frames);
  if (main_silent && sub_silent) {
    for (uint32_t i = 0; i < frames; ++i)
      main_yn[i] = 0.f;
    if (has_sub)
      for (uint32_t i = 0; i < frames; ++i)
        sub_yn[i] = 0.f;
    return;
  }

  const float inv = 1.f / (float)frames;
  const float d_drive = (drive_to - drive_from) * inv;
  const float d_shape = (shape_to - shape_from) * inv;
  const float d_gain = (gain_to - gain_from) * inv;
  float drive = drive_from;
  float shape = shape_from;
  float gain = gain_from;

  for (uint32_t i = 0; i < frames; ++i) {
    drive += d_drive;
    shape += d_shape;
    gain += d_gain;
    if (!main_silent)
      main_yn[i] = gain * fold(main_xn[i], drive, shape);
    else
      main_yn[i] = 0.f;
    if (has_sub) {
      if (!sub_silent)
        sub_yn[i] = gain * fold(sub_xn[i], drive, shape);
      else
        sub_yn[i] = 0.f;
    }
  }
}

void MODFX_PARAM(uint8_t index, int32_t value) {
  // A program that has never held this effect sends zeroes for its knobs, and
  // zero is a drive of 1x with the hard curve: the folder would look dead until
  // a knob moved. Until one of them is not zero it keeps its own positions.
  if (!program_start::chosen(value))
    return;

  const float v = clipminmaxf(0.f, (float)value * (1.f / 1023.f), 1.f);

  switch (index) {
  case k_user_modfx_param_time:
    s.drive_target = kDriveMin * arc_pow2f(kDriveSpan * v);
    return;

  case k_user_modfx_param_depth:
    s.shape_target = v;
    return;

  default:
    return;
  }
}

void MODFX_SUSPEND(void) {}

void MODFX_RESUME(void) {}
