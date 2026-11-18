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
 * Edit 6   Trans        transient: one continuous morph from mallet to knock
 *                       (see below)
 * Edit 7   --           the mkI osc has six edit parameters, so the shape lives
 *                       in the Trans band instead of on its own knob
 *
 * The transient shapes are matched to the bodies and resonators of the other
 * units, so the oscillator can excite them the way the real object is hit: a
 * soft mallet on a membrane or a plate (mallet), a stick on a wooden bar or a
 * steel plate (stick), a fingertip on a string (pluck), a metal tine or a bell
 * (ping) and a knuckle on a wooden body (knock).
 *
 * Trans sweeps through them: the five shapes are the five points of one
 * continuous morph, and everywhere in between the two neighbouring shapes are
 * mixed, coefficient by coefficient. Nothing is turned off at a boundary, so a
 * sweep is a sweep and not a walk past four silent gaps, and the knob has a
 * shape of its own at every position instead of only at five of them.
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
#include "../../common/program_start.h"

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
// Small, libm free helpers. One libm call is enough to pull newlib's reentrancy
// struct (about a kilobyte of RAM) and its errno machinery into the unit, and
// the oscillator has no use for either. The oscillator API has no power of two,
// so this one splits the exponent off and evaluates the fraction itself.
inline float pow2f(float x) {
  int i = (int)x;
  if (x < 0.f && (float)i != x)
    --i;
  const float t = (x - (float)i) * 0.69314718f;   // 2^r = e^(r ln 2), r in [0,1)
  const float p = 1.f + t * (1.f + t * (0.5f + t * (0.16666667f +
                  t * (0.04166667f + t * (0.00833333f + t * 0.00138889f)))));
  union { float f; uint32_t u; } scale;
  scale.u = (uint32_t)(i + 127) << 23;
  return p * scale.f;
}

inline float expf_fast(float x) { return pow2f(x * 1.44269504f); }

// e^-a, with the small exponents an envelope or a one pole needs.
inline float expNeg(float a) {
  if (a < 0.25f)
    return 1.f - a + a * a * (0.5f - a * 0.16666667f);
  return pow2f(-1.44269504f * a);
}

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

constexpr float kOutGain = 0.5f;

enum TransType {
  kTransMallet = 0,
  kTransStick,
  kTransPluck,
  kTransPing,
  kTransKnock,
  kTransCount
};

// One row per transient shape, the five points of the morph. fc1 / fc2 are
// one-pole low passes on the burst (0 = no filter), dry1 is raw burst mixed in
// on top, tau1 / tau2 the decay of the fast and the slow envelope, ring the
// share of the tuned resonator that makes the metal end, and gain normalises
// the peak so the points of the sweep are equally loud.
struct TransShape {
  float fc1;
  float dry1;
  float wet1;
  float tau1;
  float fc2;
  float wet2;
  float tau2;
  float gain;
  float ring;
};

// The gains were measured so that all five land at the same attack level (rms
// of the first ten milliseconds around 0.45), which keeps the sweep from
// changing level while it changes shape.
const TransShape kTransShape[kTransCount] = {
    // Soft mallet head: no high end, a low thump with a little body to it.
    {900.f, 0.f, 1.f, 0.014f, 0.f, 0.f, 0.f, 4.3f, 0.f},
    // Hard stick: full band and gone in a few milliseconds.
    {0.f, 1.f, 0.f, 0.003f, 0.f, 0.f, 0.f, 2.0f, 0.f},
    // Fingertip on a string: a bright edge plus the woody body under it.
    {5000.f, 0.55f, 0.45f, 0.003f, 620.f, 0.9f, 0.018f, 2.25f, 0.f},
    // Metal: the ring is tuned to the note and sits on top of a short click.
    {0.f, 0.25f, 0.75f, 0.030f, 0.f, 0.f, 0.f, 3.5f, 1.f},
    // Knuckle on a body: only the low end, slow enough to read as a thump.
    {380.f, 0.f, 1.f, 0.026f, 170.f, 0.6f, 0.040f, 4.3f, 0.f},
};

// The same shapes as per-sample coefficients. They are built once at load time,
// so turning the knob only interpolates two rows instead of calling expf().
struct TransCoef {
  float coef1;
  float dry1;
  float wet1;
  float exp1;
  float coef2;
  float wet2;
  float exp2;
  float gain;
  float ring;
};

TransCoef g_trans_coef[kTransCount];

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
  float shape;        // the A knob plus the LFO, 0..1
  float shape_base;   // the A knob alone
  float lfo;          // smoothed LFO offset handed over by the synth
  float shift;   // B knob, 0..1
  float trans;   // 0..1, transient knob: where in the morph to sit

  // Derived from the user parameters.
  float velvet_density;  // impulses per sample
  float crackle_rate;    // events per sample
  float grain_coef;      // decay coefficient of a crackle grain
  float grain_lp;        // one-pole smoothing of grains / sample & hold
  float svf_k;           // 1 / Q of the tracking band-pass
  float sh_ratio;        // sample & hold rate, as a multiple of the note
  float hybrid_mix;      // band-pass <-> sine
  float velvet_gain;     // compensates the impulse density
  float svf_gain;        // output trim of the tracking band-pass
  float sh_inc;          // sample & hold phase step per sample
  float trans_ring;      // 0..1, share of the tuned ring in the burst
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
  if (s.trans_ring <= 0.f)
    return;

  const float fs = (float)k_samplerate;
  const float fc = clipminmaxf(280.f, s.freq * 6.5f, 9000.f);
  const float r = expNeg(1.f / (kTransShape[kTransPing].tau1 * fs));
  s.ping_a1 = 2.f * r * osc_cosf(0.5f * fc / fs);
  s.ping_a2 = -r * r;
  s.ping_b0 = (1.f - r) * 0.5f;
}

void buildTransCoef(const TransShape &t, TransCoef &c) {
  const float fs = (float)k_samplerate;
  // A shape without a low pass is a pass-through, not a filter at 0 Hz: that is
  // what makes the coefficients safe to interpolate.
  c.coef1 = (t.fc1 > 0.f) ? 1.f - expNeg(2.f * kPi * t.fc1 / fs) : 1.f;
  c.coef2 = (t.fc2 > 0.f) ? 1.f - expNeg(2.f * kPi * t.fc2 / fs) : 1.f;
  c.dry1 = t.dry1;
  c.wet1 = t.wet1;
  c.wet2 = t.wet2;
  c.gain = t.gain;
  c.ring = t.ring;
  c.exp1 = expNeg(1.f / (t.tau1 * fs));
  c.exp2 = (t.tau2 > 0.f) ? expNeg(1.f / (t.tau2 * fs)) : 0.f;
}

void buildTransCoefs() {
  for (int i = 0; i < kTransCount; ++i)
    buildTransCoef(kTransShape[i], g_trans_coef[i]);
}

// Between two shapes the burst runs two paths at once (a filtered and a dry one,
// or a click and a ring), which is up to 4 dB louder in the middle than at the
// shapes themselves. This is the measured level of the sweep at every quarter
// step, relative to the shapes: the knob changes the shape and not the volume.
// The values at the five whole steps are exactly 1, so the five shapes are
// untouched and only the ground between them is trimmed.
const float kSweepTrim[17] = {
    1.000f, 0.644f, 0.617f, 0.706f, 1.000f, 1.029f, 1.038f, 1.078f, 1.000f,
    0.977f, 0.863f, 0.824f, 1.000f, 0.703f, 0.612f, 0.640f, 1.000f};

void updateTransient() {
  // Where the knob is in the sweep, and how far between its two neighbours.
  const float u = s.trans * (float)(kTransCount - 1);
  int i = (int)u;
  if (i > kTransCount - 2)
    i = kTransCount - 2;
  const float t = u - (float)i;

  const TransCoef &a = g_trans_coef[i];
  const TransCoef &b = g_trans_coef[i + 1];

  s.trans_lp_coef = linintf(t, a.coef1, b.coef1);
  s.trans_lp2_coef = linintf(t, a.coef2, b.coef2);
  s.trans_wet1 = linintf(t, a.wet1, b.wet1);
  s.trans_dry1 = linintf(t, a.dry1, b.dry1);
  s.trans_wet2 = linintf(t, a.wet2, b.wet2);
  s.trans_coef = linintf(t, a.exp1, b.exp1);

  const float q = u * 4.f;  // 0..16, the same scale as the trim table
  int qi = (int)q;
  if (qi > 15)
    qi = 15;
  const float trim = linintf(q - (float)qi, kSweepTrim[qi], kSweepTrim[qi + 1]);
  s.trans_gain = linintf(t, a.gain, b.gain) * trim;

  // Once the morph has left the metal end, the ring is frozen: clear it so a
  // later sweep back into it starts from silence instead of from old state.
  const float ring = linintf(t, a.ring, b.ring);
  if (ring <= 0.f && s.trans_ring > 0.f) {
    s.trans_r1 = 0.f;
    s.trans_r2 = 0.f;
  }
  s.trans_ring = ring;
  s.trans_decay2 = linintf(t, a.exp2, b.exp2);

  updatePing();
}

// A filtered impulse train loses level quickly as the grains get longer, so the
// compensation follows both the density and the smoothing.
void updateVelvetGain() {
  s.velvet_gain = clipminmaxf(
      0.5f, 1.15f / sqrtf_fast(s.velvet_density * s.grain_lp * 0.5f), 600.f);
}

// The knobs update only what they change: a repeat of the old single pass over
// everything cost twelve expf() and a sqrtf() per event, most of them for values
// that had not moved.
void updateShape() {
  // Impulse / event densities, per sample.
  s.velvet_density = clipminmaxf(0.f, 0.0005f * expf_fast(6.00f * s.shape), 0.25f);
  s.crackle_rate = clipminmaxf(0.f, 0.00002f * expf_fast(8.50f * s.shape), 0.2f);

  // Sample & hold rate, 1x .. 64x the note frequency.
  s.sh_ratio = expf_fast(4.16f * s.shape);

  // Tracking band-pass: Q from 1.5 up to 60, and the output trim that goes with
  // it. Both are block constants, so they are not recomputed per sample.
  s.svf_k = 1.f / (1.5f * expf_fast(3.70f * s.shape));
  s.svf_gain = clipminmaxf(0.8f, 0.35f * sqrtf_fast(1.f / s.svf_k), 6.f);

  s.hybrid_mix = s.shape;

  updateVelvetGain();
}

void updateGrain() {
  // 0.5 ms .. 75 ms smoothing of impulses and sample & hold steps.
  const float smooth_tau = 0.0005f * expf_fast(5.01f * s.grain);
  s.grain_lp = 1.f - expNeg(1.f / (smooth_tau * (float)k_samplerate));

  // 2 ms .. 400 ms decay of a crackle grain.
  const float decay_tau = 0.002f * expf_fast(5.30f * s.grain);
  s.grain_coef = expNeg(1.f / (decay_tau * (float)k_samplerate));

  updateVelvetGain();
}

void updateDerived() {
  updateShape();
  updateGrain();
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
  s.sh_inc = s.w0 * s.sh_ratio;

  updatePing();

  const float fc = clipminmaxf(20.f, s.freq, 18000.f);
  const float g = osc_tanpif(clipminmaxf(0.0001f, fc / (float)k_samplerate, 0.45f));
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
  s.sh_phase += s.sh_inc;
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

  return v1 * s.svf_gain;
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
  if (s.trans_env <= 1e-4f)
    return 0.f;

  const float n = osc_white();
  const float env = s.trans_env;

  // The colour of the burst. The low pass is what a soft head does to the hit,
  // the ring is the metal end of the sweep; the morph crossfades the two, so a
  // shape between pluck and ping is half felt and half tine.
  s.trans_lp += (n - s.trans_lp) * s.trans_lp_coef;
  float tone = s.trans_lp;

  if (s.trans_ring > 0.f) {
    const float y =
        s.ping_b0 * n + s.ping_a1 * s.trans_r1 + s.ping_a2 * s.trans_r2;
    s.trans_r2 = s.trans_r1;
    s.trans_r1 = y;
    tone += (y - tone) * s.trans_ring;
  }

  float v = (s.trans_wet1 * tone + s.trans_dry1 * n) * env;

  // The slow under-layer: the body tone of a pluck, the low thump of a knock.
  if (s.trans_wet2 > 0.f) {
    s.trans_lp2 += (n - s.trans_lp2) * s.trans_lp2_coef;
    v += s.trans_wet2 * s.trans_lp2 * s.trans_env2;
    s.trans_env2 *= s.trans_decay2;
  }

  s.trans_env *= s.trans_coef;
  return v * s.trans_gain;
}

/*---------------------------------------------------------------------------*
 * Block rendering
 *
 * The algorithm is a template parameter, so it is dispatched once per block
 * instead of once per sample, and everything that only moves with a knob is
 * read once per block as well.
 *---------------------------------------------------------------------------*/

template <int A>
inline float noiseSample();

template <>
inline float noiseSample<kAlgoWhite>() {
  return osc_white();
}

template <>
inline float noiseSample<kAlgoPink>() {
  return pinkNoise();
}

template <>
inline float noiseSample<kAlgoBrown>() {
  return brownNoise();
}

template <>
inline float noiseSample<kAlgoVelvet>() {
  return velvetNoise();
}

template <>
inline float noiseSample<kAlgoCrackle>() {
  return crackleNoise();
}

template <>
inline float noiseSample<kAlgoStep>() {
  return stepNoise();
}

template <>
inline float noiseSample<kAlgoTuned>() {
  return tunedNoise();
}

template <>
inline float noiseSample<kAlgoHybrid>() {
  return hybridNoise();
}

template <int A>
void renderBlock(q31_t *__restrict y, const uint32_t frames) {
  const float gain = kAlgoGain[A];
  const float tilt = s.shift;
  const float lo_gain = 1.5f - tilt;
  const float hi_gain = 0.25f + 1.5f * tilt;
  const float level = s.level;
  const float tilt_coef = 0.16f;  // one-pole around 1.3 kHz at 48 kHz

  q31_t *const y_e = y + frames;

  for (; y != y_e; ++y) {
    float x = noiseSample<A>() * gain;

    // The transient goes in before the tilt, so the osc keeps its own colour.
    x += transientSample();

    // Spectral tilt: crossfade between the low and the high part of the signal.
    s.tilt_lp += (x - s.tilt_lp) * tilt_coef;
    const float lo = s.tilt_lp;
    const float hi = x - lo;
    x = lo * lo_gain + hi * hi_gain;

    // DC blocker, needed for the integrator based algorithms.
    const float dc = x - s.dc_x + 0.995f * s.dc_y;
    s.dc_x = x;
    s.dc_y = dc;

    *y = f32_to_q31(osc_softclipf(0.08f, dc * level * kOutGain));
  }
}

}  // namespace

/*---------------------------------------------------------------------------*
 * Unit hooks
 *---------------------------------------------------------------------------*/

void OSC_INIT(uint32_t platform, uint32_t api) {
  program_start::reset();
  (void)platform;
  (void)api;

  s.algo = kAlgoWhite;
  s.tune = 0.f;
  s.keytrk = 1.f;
  s.grain = 0.35f;
  s.level = 0.8f;
  s.shape = 0.5f;
  s.shape_base = 0.5f;
  s.lfo = 0.f;
  s.shift = 0.5f;
  s.trans = 0.25f;  // the stick point of the sweep: a click for any material

  s.rng = 0x9E3779B9u ^ osc_mcu_hash();
  if (s.rng == 0u)
    s.rng = 0x1234567u;

  s.w0 = 0.f;
  s.freq = 440.f;

  buildTransCoefs();
  updateDerived();
  resetRuntime();
}

void OSC_CYCLE(const user_osc_param_t *const params, int32_t *yn,
               const uint32_t frames) {
  updatePitch(params);

  // The synth hands its LFO to the unit as an offset on the shape parameter,
  // the same way the factory oscillator adds it to its wavetable position. Half
  // of it is enough to move the character across the whole knob, and it is
  // smoothed so a fast LFO does not step the filters block by block.
  s.lfo += (q31_to_f32(params->shape_lfo) - s.lfo) * 0.10f;
  const float sh = clipminmaxf(0.f, s.shape_base + 0.5f * s.lfo, 1.f);
  if (fabsf(sh - s.shape) > 0.0015f) {
    s.shape = sh;
    updateShape();
  }

  q31_t *__restrict y = (q31_t *)yn;

  switch (s.algo) {
  case kAlgoPink:
    renderBlock<kAlgoPink>(y, frames);
    break;
  case kAlgoBrown:
    renderBlock<kAlgoBrown>(y, frames);
    break;
  case kAlgoVelvet:
    renderBlock<kAlgoVelvet>(y, frames);
    break;
  case kAlgoCrackle:
    renderBlock<kAlgoCrackle>(y, frames);
    break;
  case kAlgoStep:
    renderBlock<kAlgoStep>(y, frames);
    break;
  case kAlgoTuned:
    renderBlock<kAlgoTuned>(y, frames);
    break;
  case kAlgoHybrid:
    renderBlock<kAlgoHybrid>(y, frames);
    break;
  case kAlgoWhite:
  default:
    renderBlock<kAlgoWhite>(y, frames);
    break;
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
  // A program that has never held this unit sends zeroes for its six edit knobs
  // and for A and B, and zero is a level of zero or a bow with no pressure: the
  // unit would look dead until a knob moved. A and B are latched on their own so
  // one of them arriving as zero cannot mute the level knobs.
  if (index == k_user_osc_param_shape || index == k_user_osc_param_shiftshape) {
    if (!program_start::chosen_shape((int32_t)value))
      return;
  } else if (index >= k_user_osc_param_id1 && index <= k_user_osc_param_id6 &&
             !program_start::chosen((int32_t)value)) {
    return;
  }

  switch (index) {
  case k_user_osc_param_shape:
    s.shape_base = param_val_to_f32(value);
    s.shape = clipminmaxf(0.f, s.shape_base + 0.5f * s.lfo, 1.f);
    updateShape();
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
    updateGrain();
    break;

  case k_user_osc_param_id5:
    s.level = clipminmaxf(0.f, (float)value * 0.01f, 1.f);
    break;

  case k_user_osc_param_id6:
    s.trans = clipminmaxf(0.f, (float)value * 0.01f, 1.f);
    updateTransient();
    break;

  default:
    break;
  }
}
