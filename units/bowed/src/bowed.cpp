// bowed — a sustained oscillator: a string (or a pipe, or a sheet of metal) that
// is driven continuously instead of struck.
//
// Every other unit here answers a hit. This one keeps its energy going: a delay
// loop of one period is fed by a bow that rubs it, and the loop's losses decide
// how long it takes to speak and how it dies when the bow leaves the string.
// Four voices, because a chord of sustained notes is the point.
//
// The API is the oscillator one, so the knobs are the six parameters in
// manifest.json (A and B are shape and shift-shape, as on every osc unit).
#include <math.h>
#include <stdint.h>

#include "userosc.h"

#include "../../common/arcmath.h"

namespace {

constexpr int kVoices = 4;
constexpr int kMaxPeriod = 1024;      // 47 Hz at 48 kHz
constexpr float kFs = 48000.f;
constexpr float kPi = 3.14159265358979f;
constexpr float kDcCoef = 6.2831853f * 12.f / 48000.f;  // 12 Hz

// A model is the loop's character: how lossy it is, where its body sits, and how
// much the bow's own noise gets through. The four are a string, a struck-but-
// sustained metal sheet, a pipe, and a hum, which is a string with the body
// taken out.
struct Model {
  float loss;      // loop gain: how long it rings after the bow stops
  float damp;      // loop lowpass: 0 is dark, 1 is bright
  float body_hz;   // where the body resonance sits
  float body_q;
  float body_mix;
  float rosin;     // how much of the bow's noise reaches the loop
  float even;      // how much of the octave sits in the excitation
};

const Model kModels[5] = {
    // loss   damp  body_hz  body_q  mix    rosin  even
    {0.9993f, 0.55f, 460.f, 2.0f, 0.55f, 0.55f, 0.15f},   // string
    {0.9997f, 0.72f, 1250.f, 3.4f, 0.40f, 0.35f, 0.05f},  // metal
    {0.9988f, 0.40f, 320.f, 1.4f, 0.70f, 0.75f, 0.35f},   // pipe
    {0.9990f, 0.30f, 700.f, 1.1f, 0.30f, 0.60f, 0.50f},   // hum
    {0.9995f, 0.85f, 2200.f, 2.6f, 0.45f, 0.20f, 0.10f},  // glass
};

struct Voice {
  float buf[kMaxPeriod];
  int   len;        // integer part of the period
  int   pos;
  float frac;       // fractional part, for the interpolating read
  float lp;         // loop lowpass state
  float dc;         // the loop's own DC, taken out at 12 Hz
  float body1_y1, body1_y2;
  float body2_y1, body2_y2;
  float env;        // bow pressure, 0..1
  float env_coef;   // how fast it comes and goes
  bool  held;
  float f0;
  int   bow_pos;   // where the bow is along its own cycle
};

struct State {
  Voice voice[kVoices];
  int   next;       // round robin
  int   model;
  float pressure;   // 0..1, the bow
  float rosin;      // 0..1
  float bright;     // 0..1
  float level;
  float attack;     // 0..1, how long the bow takes to catch
  float tune;
  float noise;      // the bow's own noise, one state
  float shape_base, lfo;
  float shape_eff;

  // The body's two resonators, as coefficients. They only depend on the model,
  // so they are computed when the model changes; the audio loop must not call
  // cosf, both because it is libm and because it is per sample.
  float b1_a1, b1_a2, b2_a1, b2_a2;
};

State s;

// Slightly cheaper than a real random: two decorrelated LFSRs, which is all the
// bow needs and keeps the code small.
uint32_t g_rng = 0x12345678u;
inline float bowNoise() {
  g_rng ^= g_rng << 13;
  g_rng ^= g_rng >> 17;
  g_rng ^= g_rng << 5;
  return (float)(int32_t)g_rng * (1.f / 2147483648.f);
}

void updateBody() {
  const Model &m = kModels[s.model];

  float sn, cs;

  const float r1 = 0.995f - 0.02f * (1.f - m.body_q * 0.2f);
  const float w1 = 2.f * kPi * clipminmaxf(40.f, m.body_hz, 0.45f * kFs) / kFs;
  arc_sincos(w1, sn, cs);
  s.b1_a1 = 2.f * r1 * cs;
  s.b1_a2 = -r1 * r1;

  const float r2 = 0.993f;
  const float w2 = clipminmaxf(0.f, 2.f * kPi * m.body_hz * 2.7f / kFs, 2.8f);
  arc_sincos(w2, sn, cs);
  s.b2_a1 = 2.f * r2 * cs;
  s.b2_a2 = -r2 * r2;
}

void clearVoice(Voice &v) {
  for (int i = 0; i < kMaxPeriod; ++i)
    v.buf[i] = 0.f;
  v.lp = 0.f;
  v.dc = 0.f;
  v.body1_y1 = v.body1_y2 = 0.f;
  v.body2_y1 = v.body2_y2 = 0.f;
  v.env = 0.f;
  v.held = false;
  v.bow_pos = 0;
  v.len = 100;
  v.pos = 0;
  v.frac = 0.f;
}

void setPeriod(Voice &v, float f0) {
  float period = kFs / f0;
  if (period > (float)(kMaxPeriod - 2))
    period = (float)(kMaxPeriod - 2);
  if (period < 2.f)
    period = 2.f;
  v.len = (int)period;
  v.frac = period - (float)v.len;
}

// Reads one period back with a linear interpolation, so a note is in tune
// between the integer delay lengths.
inline float loopRead(const Voice &v) {
  int p = v.pos - v.len;
  if (p < 0)
    p += kMaxPeriod;
  int q = p - 1;
  if (q < 0)
    q += kMaxPeriod;
  return v.buf[p] + (v.buf[q] - v.buf[p]) * v.frac;
}

void strike(int note, float fine) {
  Voice &v = s.voice[s.next];
  s.next = (s.next + 1) % kVoices;
  clearVoice(v);
  const float n = (float)note + fine + s.tune;
  v.f0 = 440.f * arc_pow2f((n - 69.f) * (1.f / 12.f));
  setPeriod(v, v.f0);
  v.held = true;
  // The bow does not catch the string instantly: at a low pressure it takes
  // longer, which is most of what a slow bow attack sounds like.
  const float t = 0.004f + 0.35f * s.attack * (1.2f - s.pressure);
  v.env_coef = 1.f - arc_pow2f(-1.f / (t * kFs) * 6.9f);
  v.env = 0.f;
  // A little something in the loop to start it, or a silent loop stays silent.
  for (int i = 0; i < kMaxPeriod; ++i)
    v.buf[i] = 0.002f * bowNoise();
}

}  // namespace

void OSC_INIT(uint32_t platform, uint32_t api) {
  (void)platform;
  (void)api;
  s.next = 0;
  s.model = 0;
  s.pressure = 0.55f;
  s.rosin = 0.5f;
  s.bright = 0.55f;
  s.level = 0.5f;
  s.attack = 0.35f;
  s.tune = 0.f;
  s.noise = 0.f;
  s.shape_base = 0.55f;
  s.shape_eff = 0.55f;
  s.lfo = 0.f;
  updateBody();
  for (int i = 0; i < kVoices; ++i)
    clearVoice(s.voice[i]);
}

void OSC_CYCLE(const user_osc_param_t *const params, int32_t *yn,
               const uint32_t frames) {
  // The synth's LFO arrives as an offset on the shape parameter, which here is
  // the bow pressure: the note leans into the string and lets go.
  s.lfo += (q31_to_f32(params->shape_lfo) - s.lfo) * 0.10f;
  const float sh = clipminmaxf(0.f, s.shape_base + 0.4f * s.lfo, 1.f);
  if (fabsf(sh - s.shape_eff) > 0.0015f) {
    s.shape_eff = sh;
    s.pressure = sh;
  }

  const Model &m = kModels[s.model];
  const float loop_gain = m.loss * (0.99985f + 0.00015f * s.pressure);
  const float damp = clipminmaxf(0.f, m.damp * (0.35f + 0.9f * s.bright), 0.995f);
  const float rosin = m.rosin * s.rosin;
  // The bow speed: pressure is both how hard it presses and how fast it drags.
  const float drive = 0.05f + 0.5f * s.pressure;

  q31_t *__restrict y = (q31_t *)yn;

  for (uint32_t i = 0; i < frames; ++i) {
    float mix = 0.f;

    for (int vi = 0; vi < kVoices; ++vi) {
      Voice &v = s.voice[vi];
      if (!v.held && v.env < 1e-4f)
        continue;

      // The bow: pressure comes up over the attack, and lets go when the key
      // does, which is the only thing a note-off can mean to a bowed note.
      const float target = v.held ? 1.f : 0.f;
      v.env += (target - v.env) * (v.held ? v.env_coef : v.env_coef * 2.f);

      // A delay loop with any asymmetric excitation in it collects a DC offset,
      // and DC in a string is headroom spent on nothing. One pole at 12 Hz takes
      // it out of the loop itself, not just off the output.
      const float raw = loopRead(v);
      v.dc += (raw - v.dc) * kDcCoef;
      const float back = raw - v.dc;

      // The friction: a stream of noise whose density follows the bow, plus one
      // small push per period. The push has to be a pulse and not a constant: a
      // constant feeds the loop's DC, and the string then sings a drone at
      // whatever the body resonates at instead of at its own pitch.
      s.noise += (bowNoise() - s.noise) * (0.05f + 0.35f * rosin);
      float excite = s.noise * rosin * drive;
      if (v.bow_pos >= v.len) {
        v.bow_pos = 0;
        excite += (v.env * v.env) * 0.6f * drive;
      }
      ++v.bow_pos;

      // Loop losses: one pole, and a little of the octave so the even partials
      // are not perfectly absent (which is what makes a bowed string richer than
      // a plucked one).
      v.lp += (back - v.lp) * damp;
      const float next = excite + loop_gain * (v.lp + 0.05f * m.even * back);

      v.buf[v.pos] = clipminmaxf(-4.f, next, 4.f);
      if (++v.pos >= kMaxPeriod)
        v.pos = 0;

      // The body: two resonators in parallel, which is all "the wood" needs to
      // be. A string with no body is a buzz; this is what makes it an object.
      const float in = v.lp * v.env;
      const float y1 = s.b1_a1 * v.body1_y1 + s.b1_a2 * v.body1_y2 + in * 0.05f;
      v.body1_y2 = v.body1_y1;
      v.body1_y1 = y1;

      const float y2 = s.b2_a1 * v.body2_y1 + s.b2_a2 * v.body2_y2 + in * 0.03f;
      v.body2_y2 = v.body2_y1;
      v.body2_y1 = y2;

      mix += in * (1.f - m.body_mix) + (y1 + y2) * m.body_mix;
    }

    mix *= s.level * 0.25f;
    y[i] = f32_to_q31(clipminmaxf(-1.f, osc_softclipf(0.18f, mix), 1.f));
  }
}

void OSC_NOTEON(const user_osc_param_t *const params) {
  const int note = (int)(params->pitch >> 8);
  const float fine = (float)(params->pitch & 0xFFu) * (1.f / 255.f);
  strike(note, fine);
}

void OSC_NOTEOFF(const user_osc_param_t *const params) {
  (void)params;
  for (int i = 0; i < kVoices; ++i)
    s.voice[i].held = false;
}

void OSC_MUTE(const user_osc_param_t *const params) {
  (void)params;
  for (int i = 0; i < kVoices; ++i)
    clearVoice(s.voice[i]);
}

void OSC_PARAM(uint16_t index, uint16_t value) {
  switch (index) {
  case k_user_osc_param_shape:
    s.shape_base = param_val_to_f32(value);
    s.shape_eff = clipminmaxf(0.f, s.shape_base + 0.4f * s.lfo, 1.f);
    s.pressure = s.shape_eff;
    break;

  case k_user_osc_param_shiftshape:
    s.bright = param_val_to_f32(value);
    break;

  case k_user_osc_param_id1: {
    int model = (int)value;
    if (model < 0)
      model = 0;
    if (model > 4)
      model = 4;
    if (model != s.model) {
      s.model = model;
      updateBody();
    }
    break;
  }

  case k_user_osc_param_id2:  // bow pressure, 0 .. 100
    s.pressure = clipminmaxf(0.f, (float)value * 0.01f, 1.f);
    if (s.lfo == 0.f)
      s.shape_eff = s.pressure;
    break;

  case k_user_osc_param_id3:  // rosin, 0 .. 100
    s.rosin = clipminmaxf(0.f, (float)value * 0.01f, 1.f);
    break;

  case k_user_osc_param_id4:  // bipolar percent, 100 is the centre
    s.tune = ((float)value - 100.f) * 0.24f;
    break;

  case k_user_osc_param_id5:  // level, 0 .. 100
    s.level = clipminmaxf(0.f, (float)value * 0.01f, 1.f);
    break;

  case k_user_osc_param_id6:  // attack, 0 .. 100
    s.attack = clipminmaxf(0.f, (float)value * 0.01f, 1.f);
    break;

  default:
    break;
  }
}
