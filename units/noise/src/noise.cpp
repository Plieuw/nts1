/*
 * noise.cpp -- multi-algorithm noise oscillator for the KORG Nu:Tekt NTS-1.
 *
 * BSD 3-Clause License, see LICENSE.
 *
 * A-knob   (SHAPE)      algorithm character (see below)
 * B-knob   (SHIFT)      spectral tilt: dark <-> bright
 * Edit 1   Algo         white / pink / brown / velvet / crackle / step / tuned / hybrid
 * Edit 2   Tune         +/- 24 semitones
 * Edit 3   Keytrk       how much the algorithms follow the played note
 * Edit 4   Grain        length of the grains (velvet, crackle, step smoothing)
 * Edit 5   Level        output level
 * Edit 6   Trans        noise transient on every note-on, for a percussive attack
 *
 * Per-algorithm character (A-knob):
 *   white    nothing to shape, kept as reference noise
 *   pink     tilt of the pink filter bank
 *   brown    leak of the integrator: rumbly <-> airy
 *   velvet   impulse density, from sparse ticks to a smooth hiss
 *   crackle  event rate, from a few pops per second to dense crackle
 *   step     sample & hold rate as a multiple of the played note
 *   tuned    resonance of the tracking band-pass (whistle <-> wideband)
 *   hybrid   mix between the tracking band-pass and a sine at the same pitch
 */

#include "userosc.h"

namespace {

enum Algo {
  kAlgoWhite = 0,
  kAlgoPink,
  kAlgoBrown,
  kAlgoVelvet,
  kAlgoCrackle,
  kAlgoStep,
  kAlgoTuned,
  kAlgoHybrid,
  kAlgoCount
};

constexpr float kPi = 3.14159265358979f;
constexpr float kOutGain = 0.5f;

// Per algorithm output trim, measured so all eight land around -14 dBFS.
const float kAlgoGain[kAlgoCount] = {0.85f, 3.10f, 1.45f, 1.00f,
                                     1.65f, 4.10f, 2.00f, 1.30f};

struct State {
  // User parameters.
  int   algo;
  float tune;    // semitones
  float keytrk;  // 0..1
  float grain;   // 0..1
  float level;   // 0..1
  float shape;   // A knob, 0..1
  float shift;   // B knob, 0..1
  float trans;   // 0..1, note-on transient amount

  // Derived from the user parameters.
  float velvet_density;  // impulses per sample
  float crackle_rate;    // events per sample
  float grain_coef;      // decay coefficient of a crackle grain
  float grain_lp;        // one-pole smoothing of grains / sample & hold
  float svf_k;           // 1 / Q of the tracking band-pass
  float sh_ratio;        // sample & hold rate, as a multiple of the note
  float hybrid_mix;      // band-pass <-> sine
  float velvet_gain;     // compensates the impulse density
  float trans_coef;      // decay of the note-on transient

  // Runtime state.
  uint32_t rng;
  float pink[7];
  float brown;
  float env;
  float sh_phase;
  float sh_hold;
  float sh_smooth;
  float svf_a1;
  float svf_a2;
  float svf_a3;
  float ic1;
  float ic2;
  float tilt_lp;
  float dc_x;
  float dc_y;
  float phi;
  float trans_env;  // note-on transient envelope
  float w0;      // phase increment per sample of the played note
  float freq;    // in Hz
};

State s;

/*---------------------------------------------------------------------------*
 * Helpers
 *---------------------------------------------------------------------------*/

inline uint32_t xorshift() {
  uint32_t x = s.rng;
  x ^= x << 13;
  x ^= x >> 17;
  x ^= x << 5;
  s.rng = x;
  return x;
}

inline float rand01() {
  return (float)(xorshift() >> 8) * (1.f / 16777216.f);
}

inline float randbi() {
  return rand01() * 2.f - 1.f;
}

void resetRuntime() {
  for (int i = 0; i < 7; ++i)
    s.pink[i] = 0.f;
  s.brown = 0.f;
  s.env = 0.f;
  s.sh_phase = 0.f;
  s.sh_hold = 0.f;
  s.sh_smooth = 0.f;
  s.ic1 = 0.f;
  s.ic2 = 0.f;
  s.tilt_lp = 0.f;
  s.dc_x = 0.f;
  s.dc_y = 0.f;
  s.phi = rand01();
  s.trans_env = 0.f;
}

void updateDerived() {
  // 0.5 ms .. 75 ms smoothing of impulses and sample & hold steps.
  const float smooth_tau = 0.0005f * expf(5.01f * s.grain);
  s.grain_lp = 1.f - expf(-1.f / (smooth_tau * (float)k_samplerate));

  // 2 ms .. 400 ms decay of a crackle grain.
  const float decay_tau = 0.002f * expf(5.30f * s.grain);
  s.grain_coef = expf(-1.f / (decay_tau * (float)k_samplerate));

  // Impulse / event densities, per sample.
  s.velvet_density = clipminmaxf(0.f, 0.0005f * expf(6.00f * s.shape), 0.25f);
  s.crackle_rate = clipminmaxf(0.f, 0.00002f * expf(8.50f * s.shape), 0.2f);

  // Sample & hold rate, 1x .. 64x the note frequency.
  s.sh_ratio = expf(4.16f * s.shape);

  // Tracking band-pass: Q from 1.5 up to 60.
  s.svf_k = 1.f / (1.5f * expf(3.70f * s.shape));

  s.hybrid_mix = s.shape;

  // A filtered impulse train loses level quickly as the grains get longer, so
  // the compensation follows both the density and the smoothing.
  s.velvet_gain = clipminmaxf(
      0.5f, 1.15f / sqrtf(s.velvet_density * s.grain_lp * 0.5f), 600.f);

  // A struck source: full band, gone within ten milliseconds or so. A little
  // longer than a click, short enough to read as an attack and not as a tone.
  s.trans_coef = expf(-1.f / (0.011f * (float)k_samplerate));
}

// Pitch and band-pass coefficients are refreshed once per buffer.
void updatePitch(const user_osc_param_t *params) {
  const float note = (float)(params->pitch >> 8) +
                     (float)(params->pitch & 0xFFu) * (1.f / 255.f);
  const float n = 60.f + s.keytrk * (note - 60.f) + s.tune;
  const float cn = clipminmaxf(0.f, n, 150.f);
  const uint8_t ni = (uint8_t)cn;
  const uint8_t nf = (uint8_t)((cn - (float)ni) * 255.f);

  s.w0 = osc_w0f_for_note(ni, nf);
  s.freq = s.w0 * (float)k_samplerate;

  const float fc = clipminmaxf(20.f, s.freq, 18000.f);
  const float g = tanf(kPi * fc / (float)k_samplerate);
  const float a1 = 1.f / (1.f + g * (g + s.svf_k));
  s.svf_a1 = a1;
  s.svf_a2 = g * a1;
  s.svf_a3 = g * s.svf_a2;
}

/*---------------------------------------------------------------------------*
 * Noise algorithms, all returning a roughly unit level signal
 *---------------------------------------------------------------------------*/

inline float pinkNoise() {
  const float w = osc_white();
  float *p = s.pink;
  p[0] = 0.99886f * p[0] + w * 0.0555179f;
  p[1] = 0.99332f * p[1] + w * 0.0750759f;
  p[2] = 0.96900f * p[2] + w * 0.1538520f;
  p[3] = 0.86650f * p[3] + w * 0.3104856f;
  p[4] = 0.55000f * p[4] + w * 0.5329522f;
  p[5] = -0.7616f * p[5] - w * 0.0168980f;

  const float out = p[0] + p[1] + p[2] + p[3] + p[4] + p[5] + p[6] + w * 0.5362f;
  p[6] = w * 0.115926f;

  return out * 0.11f;
}

inline float brownNoise() {
  // A-knob sets the leak: a shorter memory gives a brighter, airier sound.
  const float leak = 0.9995f - 0.0005f * s.shape;
  s.brown = (s.brown + 0.02f * osc_white()) * leak;
  return s.brown * 3.5f;
}

inline float velvetNoise() {
  const float impulse = (rand01() < s.velvet_density) ? randbi() : 0.f;
  s.env += (impulse - s.env) * s.grain_lp;
  return s.env * s.velvet_gain;
}

inline float crackleNoise() {
  if (rand01() < s.crackle_rate)
    s.env = randbi() * (0.25f + 0.75f * rand01());
  s.env *= s.grain_coef;
  return s.env * 2.2f;
}

inline float stepNoise() {
  s.sh_phase += s.w0 * s.sh_ratio;
  while (s.sh_phase >= 1.f) {
    s.sh_phase -= 1.f;
    s.sh_hold = randbi();
  }
  s.sh_smooth += (s.sh_hold - s.sh_smooth) * s.grain_lp;
  return s.sh_smooth;
}

inline float tunedNoise() {
  const float x = osc_white();

  const float v3 = x - s.ic2;
  const float v1 = s.svf_a1 * s.ic1 + s.svf_a2 * v3;
  const float v2 = s.ic2 + s.svf_a2 * s.ic1 + s.svf_a3 * v3;
  s.ic1 = 2.f * v1 - s.ic1;
  s.ic2 = 2.f * v2 - s.ic2;

  return v1 * clipminmaxf(0.8f, 0.35f * sqrtf(1.f / s.svf_k), 6.f);
}

inline float hybridNoise() {
  s.phi += s.w0;
  s.phi -= (uint32_t)s.phi;

  const float m = s.hybrid_mix;
  return (1.f - m) * tunedNoise() + m * osc_sinf(s.phi);
}

}  // namespace

/*---------------------------------------------------------------------------*
 * Unit hooks
 *---------------------------------------------------------------------------*/

void OSC_INIT(uint32_t platform, uint32_t api) {
  (void)platform;
  (void)api;

  s.algo = kAlgoWhite;
  s.tune = 0.f;
  s.keytrk = 1.f;
  s.grain = 0.35f;
  s.level = 0.8f;
  s.shape = 0.5f;
  s.shift = 0.5f;
  s.trans = 0.35f;

  s.rng = 0x9E3779B9u ^ osc_mcu_hash();
  if (s.rng == 0u)
    s.rng = 0x1234567u;

  s.w0 = 0.f;
  s.freq = 440.f;

  updateDerived();
  resetRuntime();
}

void OSC_CYCLE(const user_osc_param_t *const params, int32_t *yn,
               const uint32_t frames) {
  updatePitch(params);

  const float tilt = s.shift;
  const float tilt_coef = 0.16f;  // one-pole around 1.3 kHz at 48 kHz

  q31_t *__restrict y = (q31_t *)yn;
  const q31_t *const y_e = y + frames;

  for (; y != y_e;) {
    float x;

    switch (s.algo) {
    case kAlgoPink:
      x = pinkNoise();
      break;
    case kAlgoBrown:
      x = brownNoise();
      break;
    case kAlgoVelvet:
      x = velvetNoise();
      break;
    case kAlgoCrackle:
      x = crackleNoise();
      break;
    case kAlgoStep:
      x = stepNoise();
      break;
    case kAlgoTuned:
      x = tunedNoise();
      break;
    case kAlgoHybrid:
      x = hybridNoise();
      break;
    case kAlgoWhite:
    default:
      x = osc_white();
      break;
    }

    x *= kAlgoGain[s.algo];

    // Note-on transient: a short burst of the same broadband noise, so it is
    // shaped by the tilt and the rest of the unit just like the main signal.
    if (s.trans_env > 1e-4f) {
      x += s.trans_env * s.trans * 2.2f * osc_white();
      s.trans_env *= s.trans_coef;
    }

    // Spectral tilt: crossfade between the low and the high part of the signal.
    s.tilt_lp += (x - s.tilt_lp) * tilt_coef;
    const float lo = s.tilt_lp;
    const float hi = x - lo;
    x = lo * (1.5f - tilt) + hi * (0.25f + 1.5f * tilt);

    // DC blocker, needed for the integrator based algorithms.
    const float dc = x - s.dc_x + 0.995f * s.dc_y;
    s.dc_x = x;
    s.dc_y = dc;

    *(y++) = f32_to_q31(osc_softclipf(0.08f, dc * s.level * kOutGain));
  }
}

void OSC_NOTEON(const user_osc_param_t *const params) {
  (void)params;
  resetRuntime();
  s.trans_env = 1.f;
}

void OSC_NOTEOFF(const user_osc_param_t *const params) {
  (void)params;
}

void OSC_MUTE(const user_osc_param_t *const params) {
  (void)params;
  resetRuntime();
}

void OSC_PARAM(uint16_t index, uint16_t value) {
  switch (index) {
  case k_user_osc_param_shape:
    s.shape = param_val_to_f32(value);
    break;

  case k_user_osc_param_shiftshape:
    s.shift = param_val_to_f32(value);
    break;

  case k_user_osc_param_id1: {
    const int algo = (int)clipminmaxf(0.f, (float)value, (float)(kAlgoCount - 1));
    if (algo != s.algo) {
      s.algo = algo;
      resetRuntime();
    }
    break;
  }

  case k_user_osc_param_id2:
    s.tune = ((float)value - 100.f) * 0.24f;  // 0..200 -> -24..+24 semitones
    break;

  case k_user_osc_param_id3:
    s.keytrk = clipminmaxf(0.f, (float)value * 0.01f, 1.f);
    break;

  case k_user_osc_param_id4:
    s.grain = clipminmaxf(0.f, (float)value * 0.01f, 1.f);
    break;

  case k_user_osc_param_id5:
    s.level = clipminmaxf(0.f, (float)value * 0.01f, 1.f);
    break;

  case k_user_osc_param_id6:
    s.trans = clipminmaxf(0.f, (float)value * 0.01f, 1.f);
    break;

  default:
    break;
  }

  updateDerived();
}
