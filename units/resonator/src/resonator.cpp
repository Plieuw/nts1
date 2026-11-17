/*
 * resonator.cpp -- multi model modal resonator for the KORG Nu:Tekt NTS-1.
 *
 * BSD 3-Clause License, see LICENSE.
 *
 * The modulation slot has `elements`, which is the same idea with the pitch set
 * by hand, because a modulation effect gets two knobs and no note. This is the
 * same engine in the oscillator slot, where the note number is available: five
 * models to choose from, played from the keyboard, held by the amp envelope and
 * free to be layered with any effect after it.
 *
 * The models are the objects the other units imitate:
 *
 *   0  string     harmonic series with the inharmonic stretch a stiff string has
 *   1  membrane   Bessel ratios of a circular membrane
 *   2  plate      two dimensional bending modes of a flat plate
 *   3  bell       strongly inharmonic partials, the high ones carry the character
 *   4  multi      the first modes of all four superimposed into one dense bank
 *
 * A note is struck: a burst of filtered noise goes into the bank, the mode
 * levels depend on where the strike lands (a real bar has nodes that a strike
 * can miss), and the modes ring out with the high ones dying faster than the
 * low ones unless the brightness knob says otherwise.
 *
 * A-knob (SHAPE)   decay, 0.1 s .. 12 s
 * B-knob (SHIFT)   brightness: how long the high modes live, and how open the
 *                  strike sounds
 * Edit 1 Model     0 string, 1 membrane, 2 plate, 3 bell, 4 multi
 * Edit 2 Strike    soft mallet .. hard hammer
 * Edit 3 Pos       where the strike lands, 0 .. 1 across the object
 * Edit 4 Tune      +- 24 semitones
 * Edit 5 Level
 * Edit 6 Damp      extra damping of the high modes
 */

#include "userosc.h"

namespace {

constexpr int kModes = 16;
constexpr int kModels = 5;
constexpr float kFs = 48000.f;

// The bank normally sums to well below one; anything past this is a runaway.
constexpr float kBankLimit = 100.f;

// How hard the strike drives the bank. A resonator answers a burst of L samples
// with about L times this per mode, so it is small on purpose.
constexpr float kStrikeGain = 0.0018f;

// The attack of a note peaks at about this, before the level knob.
constexpr float kOutSum = 0.008f;

// Small, libm free helpers: the oscillator budget is tight and one call into the
// maths library would pull newlib's reentrancy struct in with it.
// 2^x: the oscillator API has no table for it, so the exponent is split off
// and the fraction is a short polynomial (2^r = e^(r ln 2), error below 1e-6).
inline float pow2f(float x) {
  int i = (int)x;
  if (x < 0.f && (float)i != x)
    --i;
  const float t = (x - (float)i) * 0.69314718f;
  const float p = 1.f + t * (1.f + t * (0.5f + t * (0.16666667f +
                  t * (0.04166667f + t * (0.00833333f + t * 0.00138889f)))));
  union { float f; uint32_t u; } scale;
  scale.u = (uint32_t)(i + 127) << 23;
  return p * scale.f;
}

// Square root without the maths library: the integer trick seeds it within a
// few percent and three Newton steps take it to float precision.
inline float sqrtf_fast(float x) {
  union { float f; uint32_t i; } u;
  u.f = x;
  u.i = (u.i >> 1) + 0x1fbd1df5u;
  float r = u.f;
  r = 0.5f * (r + x / r);
  r = 0.5f * (r + x / r);
  r = 0.5f * (r + x / r);
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

struct State {
  int model;
  float decay;    // 0..1, the A knob
  float bright;   // 0..1, the B knob
  float strike;   // 0..1
  float pos;      // 0..1
  float damp;     // 0..1
  float level;    // 0..1
  float tune;     // semitones
  float note;     // last note number, for the level curve
  float f0;       // fundamental in Hz the bank was built for
  float ratio[kModels][kModes];
  float amp[kModes];
  float a1[kModes];
  float a2[kModes];
  float y1[kModes];
  float y2[kModes];
  float exc_coef;      // colour of the strike
  float exc_env_coef;  // length of the strike
  float exc_lp;
  float exc_env;
  float out_gain;
  int active;          // modes below Nyquist, the rest is silent
  uint32_t rng;
};

State s;

void clearBank() {
  for (int k = 0; k < kModes; ++k) {
    s.y1[k] = 0.f;
    s.y2[k] = 0.f;
  }
}

void reset() {
  clearBank();
  s.exc_lp = 0.f;
  s.exc_env = 0.f;
}

void buildRatios() {
  for (int k = 0; k < kModes; ++k) {
    const float n = (float)(k + 1);
    // A real string is stiff: f_n = n * f0 * sqrt(1 + B n^2), which stretches
    // the partials apart as they go up.
    s.ratio[0][k] = n * sqrtf_fast(1.f + 0.0008f * n * n);
    s.ratio[1][k] = kMembrane[k];
    s.ratio[2][k] = kPlate[k];
    s.ratio[3][k] = kBell[k];
  }

  // The multi model takes the first four modes of each of the four objects and
  // sorts them: four fundamentals land on top of each other and the rest fills
  // in between, which is what makes it read as one dense metal rather than as
  // four instruments at once. Sorted, because the render loop stops at the
  // first mode above Nyquist.
  float merged[kModes];
  merged[0] = 1.f;   // one shared fundamental, not four stacked on top of it
  int c = 1;
  for (int m = 0; m < 4 && c < kModes; ++m)
    for (int j = 1; j < kModes && c < kModes; ++j)
      merged[c++] = s.ratio[m][j];

  for (int i = 1; i < kModes; ++i) {
    const float v = merged[i];
    int j = i - 1;
    for (; j >= 0 && merged[j] > v; --j)
      merged[j + 1] = merged[j];
    merged[j + 1] = v;
  }

  for (int k = 0; k < kModes; ++k)
    s.ratio[4][k] = merged[k];
}

void updateCoefficients() {
  const float *const ratio = s.ratio[s.model];
  const float tau = 0.1f * pow2f(6.9f * s.decay);   // 0.1 s .. 12 s
  const float spread = 0.32f - 0.28f * s.bright + 0.30f * s.damp;

  int active = 0;

  for (int k = 0; k < kModes; ++k) {
    const float f = s.f0 * ratio[k];

    if (f >= 0.45f * kFs) {
      s.amp[k] = 0.f;
      s.a1[k] = 0.f;
      s.a2[k] = 0.f;
      continue;
    }

    // Ascending ratios, so everything past this point is above Nyquist too.
    active = k + 1;

    const float tn = tau * pow2f(-spread * (float)k);
    const float r = expNegSmall(1.f / (tn * kFs));

    s.a1[k] = 2.f * r * osc_cosf(clipminmaxf(0.f, f / kFs, 0.99f));
    s.a2[k] = -r * r;

    // Where the strike lands: a mode with a node there is barely excited, which
    // is the difference between a bar struck in the middle and at the end.
    const float node =
        0.15f + 0.85f * fabsf(osc_sinf(0.5f * s.pos * (float)(k + 1)));

    // The strike goes straight into the mode, not through a coupling factor
    // that depends on how fast the mode decays: that way a short string is not
    // a hundred times louder than a long bell. The 1/(1 + k/2) tilt is what a
    // real strike injects, less energy into the high modes.
    // A hard hammer is a shorter burst, so it puts less energy in; a little of
    // that is compensated here so the knob changes the attack more than the
    // level.
    s.amp[k] = kStrikeGain * (1.f + 0.5f * s.strike) * node /
               (1.f + 0.5f * (float)k);
  }

  s.active = active;

  // The strike itself: a noise burst, short and bright for a hard hammer, long
  // and dark for a soft mallet.
  const float exc_fc = 300.f * pow2f(4.9f * s.strike);   // 300 Hz .. 8.9 kHz
  s.exc_coef =
      1.f - expNegSmall(6.2831853f * clipminmaxf(120.f, exc_fc, 16000.f) / kFs);
  const float env_tau = 0.010f * pow2f(-2.0f * s.strike);  // 10 ms .. 2.5 ms
  s.exc_env_coef = expNegSmall(1.f / (env_tau * kFs));

  // Every mode starts in phase, so the attack is as loud as the sum of the
  // strike amplitudes: a low note that keeps all sixteen modes, or the multi
  // model with its dense bank, would otherwise be far louder than a high note
  // that only keeps five. Normalising that sum keeps the keyboard and the
  // models even, and the level knob then sits on top of it.
  float sum = 0.f;
  for (int k = 0; k < active; ++k)
    sum += s.amp[k];
  // Below about middle C the modes sit close enough together to add up in
  // phase, and an octave higher they scatter: measured over the keyboard that
  // is a factor of seven, which no instrument does. This curve takes most of it
  // out again without flattening the register.
  const float trim =
      clipminmaxf(0.25f, pow2f(0.85f * (s.note - 60.f) * (1.f / 12.f)), 3.5f);
  s.out_gain = ((sum > 1e-9f) ? (kOutSum / sum) : 1.f) * trim;
}

void updatePitch(const user_osc_param_t *const params) {
  const uint8_t ni = (uint8_t)(params->pitch >> 8);
  const uint8_t nf = (uint8_t)(params->pitch & 0xFFu);
  s.note = (float)ni + (float)nf * (1.f / 255.f);
  const float f0 =
      osc_w0f_for_note(ni, nf) * (float)k_samplerate *
      pow2f(s.tune * (1.f / 12.f));

  // Follow the keyboard, including a glide, but do not rebuild sixteen modes
  // for a few cents of wobble: five cents is below what the ear can place.
  if (fabsf(f0 - s.f0) > s.f0 * 0.003f) {
    s.f0 = f0;
    updateCoefficients();
  }
}

void render(q31_t *__restrict y, const uint32_t frames) {
  const int modes = s.active;
  const float *const amp = s.amp;
  const float *const a1 = s.a1;
  const float *const a2 = s.a2;
  float *const y1 = s.y1;
  float *const y2 = s.y2;
  const float exc_coef = s.exc_coef;
  const float exc_env_coef = s.exc_env_coef;
  const float gain = s.out_gain * s.level;

  // A bank that has rung down to the last floats would keep the FPU on its slow
  // denormal path; at -300 dB there is nothing left to keep.
  {
    float mag = 0.f;
    for (int k = 0; k < kModes; ++k)
      mag += fabsf(y1[k]) + fabsf(y2[k]);
    if (mag < 1e-15f)
      clearBank();
  }

  float lp = s.exc_lp;
  float env = s.exc_env;
  uint32_t rng = s.rng;

  for (uint32_t i = 0; i < frames; ++i) {
    float exc = 0.f;
    if (env > 1e-7f) {
      rng ^= rng << 13;
      rng ^= rng >> 17;
      rng ^= rng << 5;
      const float noise = (float)(int32_t)rng * (1.f / 2147483648.f);
      lp += (noise - lp) * exc_coef;
      exc = lp * env;
      env *= exc_env_coef;
    } else {
      env = 0.f;
      lp = 0.f;
    }

    float acc = 0.f;
    for (int k = 0; k < modes; ++k) {
      const float v = amp[k] * exc + a1[k] * y1[k] + a2[k] * y2[k];
      y2[k] = y1[k];
      y1[k] = v;
      acc += v;
    }

    // A strike or a glide can move the modes under a bank that is still ringing
    // and make it run away; drop it rather than pass a NaN down the chain.
    if (!(acc > -kBankLimit && acc < kBankLimit)) {
      clearBank();
      acc = 0.f;
    }

    *y++ = f32_to_q31(osc_softclipf(0.08f, acc * gain));
  }

  s.exc_lp = lp;
  s.exc_env = env;
  s.rng = rng;
}

}  // namespace

/*---------------------------------------------------------------------------*
 * Unit hooks
 *---------------------------------------------------------------------------*/

void OSC_INIT(uint32_t platform, uint32_t api) {
  (void)platform;
  (void)api;

  s.model = 0;
  s.decay = 0.45f;
  s.bright = 0.55f;
  s.strike = 0.55f;
  s.pos = 0.32f;
  s.damp = 0.2f;
  s.level = 0.8f;
  s.tune = 0.f;
  s.f0 = 261.63f;  // middle C, until the first note arrives
  s.note = 60.f;

  s.rng = 0x9E3779B9u ^ osc_mcu_hash();
  if (s.rng == 0u)
    s.rng = 0x1234567u;

  buildRatios();
  updateCoefficients();
  reset();
}

void OSC_CYCLE(const user_osc_param_t *const params, int32_t *yn,
               const uint32_t frames) {
  updatePitch(params);
  render((q31_t *)yn, frames);
}

void OSC_NOTEON(const user_osc_param_t *const params) {
  updatePitch(params);
  reset();  // a strike cuts off what was still ringing
  s.exc_env = 1.f;
}

void OSC_NOTEOFF(const user_osc_param_t *const params) {
  (void)params;
}

void OSC_MUTE(const user_osc_param_t *const params) {
  (void)params;
  reset();
}

void OSC_PARAM(uint16_t index, uint16_t value) {
  switch (index) {
  case k_user_osc_param_shape:  // 10 bit
    s.decay = param_val_to_f32(value);
    updateCoefficients();
    break;

  case k_user_osc_param_shiftshape:  // 10 bit
    s.bright = param_val_to_f32(value);
    updateCoefficients();
    break;

  case k_user_osc_param_id1: {
    int model = (int)value;
    if (model < 0)
      model = 0;
    if (model > kModels - 1)
      model = kModels - 1;
    s.model = model;
    updateCoefficients();
    break;
  }

  case k_user_osc_param_id2:  // 0 .. 100
    s.strike = clipminmaxf(0.f, (float)value * 0.01f, 1.f);
    updateCoefficients();
    break;

  case k_user_osc_param_id3:  // 0 .. 100
    s.pos = clipminmaxf(0.f, (float)value * 0.01f, 1.f);
    updateCoefficients();
    break;

  case k_user_osc_param_id4:  // bipolar percent, 100 is the centre
    s.tune = ((float)value - 100.f) * 0.24f;
    updateCoefficients();
    break;

  case k_user_osc_param_id5:  // 0 .. 100
    s.level = clipminmaxf(0.f, (float)value * 0.01f, 1.f);
    break;

  case k_user_osc_param_id6:  // 0 .. 100
    s.damp = clipminmaxf(0.f, (float)value * 0.01f, 1.f);
    updateCoefficients();
    break;

  default:
    break;
  }
}
