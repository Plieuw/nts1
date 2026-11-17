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
 * Edit 6   Trans        transient: shape band and level (see below)
 * Edit 7   --           the mkI osc has six edit parameters, so the shape lives
 *                       in the Trans band instead of on its own knob
 *
 * The transient shapes are matched to the bodies and resonators of the other
 * units, so the oscillator can excite them the way the real object is hit: a
 * soft mallet on a membrane or a plate (mallet), a stick on a wooden bar or a
 * steel plate (stick), a fingertip on a string (pluck), a metal tine or a bell
 * (ping) and a knuckle on a wooden body (knock). Trans is cut into five equal
 * bands, one per shape; the level peaks in the middle of a band and falls to
 * zero at its edges, so you get every shape at every level and sweeping from
 * one to the next never jumps.
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

enum TransType {
  kTransMallet = 0,
  kTransStick,
  kTransPluck,
  kTransPing,
  kTransKnock,
  kTransCount
};

// One row per transient shape. fc1 / fc2 are one-pole low passes on the burst
// (0 = off), dry1 is raw burst mixed in on top, tau1 / tau2 the decay of the
// fast and the slow envelope, gain normalises the peak so the shapes are
// interchangeable at the same Trans setting.
struct TransShape {
  float fc1;
  float dry1;
  float wet1;
  float tau1;
  float fc2;
  float wet2;
  float tau2;
  float gain;
};

// The gains were measured so that all five land at the same attack level (rms
// of the first ten milliseconds around 0.45), which makes them interchangeable
// without a level jump.
const TransShape kTransShape[kTransCount] = {
    // Soft mallet head: no high end, a low thump with a little body to it.
    {900.f, 0.f, 1.f, 0.014f, 0.f, 0.f, 0.f, 4.3f},
    // Hard stick: full band and gone in a few milliseconds.
    {0.f, 1.f, 0.f, 0.003f, 0.f, 0.f, 0.f, 2.0f},
    // Fingertip on a string: a bright edge plus the woody body under it.
    {5000.f, 0.55f, 0.45f, 0.003f, 620.f, 0.9f, 0.018f, 2.25f},
    // Metal: the narrow ring is generated separately, this is the click on top.
    {0.f, 0.25f, 0.75f, 0.030f, 0.f, 0.f, 0.f, 3.5f},
    // Knuckle on a body: only the low end, slow enough to read as a thump.
    {380.f, 0.f, 1.f, 0.026f, 170.f, 0.6f, 0.040f, 4.3f},
};

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
  float trans;   // 0..1, transient knob: which shape and how loud
  float trans_level;  // 0..1, derived level of that shape

  // Derived from the user parameters.
  float velvet_density;  // impulses per sample
  float crackle_rate;    // events per sample
  float grain_coef;      // decay coefficient of a crackle grain
  float grain_lp;        // one-pole smoothing of grains / sample & hold
  float svf_k;           // 1 / Q of the tracking band-pass
  float sh_ratio;        // sample & hold rate, as a multiple of the note
  float hybrid_mix;      // band-pass <-> sine
  float velvet_gain;     // compensates the impulse density
  int   trans_type;      // shape of the note-on transient
  float trans_coef;      // decay of the fast transient envelope
  float trans_decay2;    // decay of the slow transient envelope
  float trans_lp_coef;   // one-pole low pass on the burst
  float trans_lp2_coef;  // second, darker one for the tail of the attack
  float trans_wet1;
  float trans_dry1;
  float trans_wet2;
  float trans_gain;
  float ping_a1;         // resonant ring for kTransPing
  float ping_a2;
  float ping_b0;

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
  float trans_env;   // fast note-on transient envelope
  float trans_env2;  // slow note-on transient envelope
  float trans_lp;
  float trans_lp2;
  float trans_r1;
  float trans_r2;
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
  s.trans_env2 = 0.f;
  s.trans_lp = 0.f;
  s.trans_lp2 = 0.f;
  s.trans_r1 = 0.f;
  s.trans_r2 = 0.f;
}

// The ring of the ping follows the note, so it stays in tune with the synth.
void updatePing() {
  if (s.trans_type != kTransPing)
    return;

  const float fs = (float)k_samplerate;
  const float fc = clipminmaxf(280.f, s.freq * 6.5f, 9000.f);
  const float r = expf(-1.f / (kTransShape[kTransPing].tau1 * fs));
  s.ping_a1 = 2.f * r * cosf(kPi * fc / fs);
  s.ping_a2 = -r * r;
  s.ping_b0 = (1.f - r) * 0.5f;
}

void updateTransient() {
  // One knob for both: five equal bands, level peaking in the middle of a band
  // and zero at its edges. That keeps every shape reachable at any level and
  // makes the boundary between two shapes silent instead of a jump.
  int zone = (int)(s.trans * (float)kTransCount);
  if (zone >= kTransCount)
    zone = kTransCount - 1;
  const float frac = s.trans * (float)kTransCount - (float)zone;
  const float level = 1.f - fabsf(2.f * frac - 1.f);

  if (zone != s.trans_type) {
    s.trans_type = zone;
    s.trans_lp = 0.f;
    s.trans_lp2 = 0.f;
    s.trans_r1 = 0.f;
    s.trans_r2 = 0.f;
  }
  s.trans_level = level;

  const TransShape &t = kTransShape[s.trans_type];
  const float fs = (float)k_samplerate;

  s.trans_lp_coef = (t.fc1 > 0.f) ? 1.f - expf(-2.f * kPi * t.fc1 / fs) : 0.f;
  s.trans_lp2_coef = (t.fc2 > 0.f) ? 1.f - expf(-2.f * kPi * t.fc2 / fs) : 0.f;
  s.trans_wet1 = t.wet1;
  s.trans_dry1 = t.dry1;
  s.trans_wet2 = t.wet2;
  s.trans_gain = t.gain;
  s.trans_coef = expf(-1.f / (t.tau1 * fs));
  s.trans_decay2 = (t.tau2 > 0.f) ? expf(-1.f / (t.tau2 * fs)) : 0.f;

  updatePing();
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

  updateTransient();
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

  updatePing();

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

// One sample of the note-on transient, shaped after the thing that would hit a
// string, a bar or a plate. Returns zero when the burst is over.
inline float transientSample() {
  // At a band edge the level is zero: stay out of the noise stream entirely, so
  // "no transient" really is no transient.
  if (s.trans_env <= 1e-4f || s.trans_level < 1e-4f)
    return 0.f;

  const float n = osc_white();
  float v;

  if (s.trans_type == kTransPing) {
    const float y =
        s.ping_b0 * n + s.ping_a1 * s.trans_r1 + s.ping_a2 * s.trans_r2;
    s.trans_r2 = s.trans_r1;
    s.trans_r1 = y;
    v = (s.trans_wet1 * y + s.trans_dry1 * n) * s.trans_env;
  } else {
    s.trans_lp += (n - s.trans_lp) * s.trans_lp_coef;
    v = (s.trans_wet1 * s.trans_lp + s.trans_dry1 * n) * s.trans_env;

    if (s.trans_lp2_coef > 0.f) {
      s.trans_lp2 += (n - s.trans_lp2) * s.trans_lp2_coef;
      v += s.trans_wet2 * s.trans_lp2 * s.trans_env2;
      s.trans_env2 *= s.trans_decay2;
    }
  }

  s.trans_env *= s.trans_coef;
  return v * s.trans_gain * s.trans_level;
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
  s.trans = 0.3f;   // middle of the stick band: a click at half level
  s.trans_type = kTransStick;

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

    // The transient goes in before the tilt, so the osc keeps its own colour.
    x += transientSample();

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
  s.trans_env2 = 1.f;
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
