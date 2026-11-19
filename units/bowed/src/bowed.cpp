// bowed — a sustained oscillator: a string (or a pipe, or a sheet of metal) that
// is driven continuously instead of struck.
//
// Every other unit here answers a hit. This one keeps its energy going: a delay
// loop of one period is fed by a bow that rubs it, and the loop's losses decide
// how long it takes to speak. When the key comes up the bow leaves at once: the
// amp EG of the NTS-1 sits after the oscillator slot and owns the tail, so the
// unit must not hold one of its own, or the release knob would be fighting a
// string that keeps ringing under it.
// One voice: the NTS-1 is monophonic, so there is never a second note to give a
// second delay loop to. A new key takes the loop, which is what a bow does to a
// string anyway.
//
// The API is the oscillator one, so the knobs are the six parameters in
// manifest.json (A and B are shape and shift-shape, as on every osc unit).
#include <math.h>
#include <stdint.h>

#include "userosc.h"
#include "../../common/program_start.h"

#include "../../common/arcmath.h"

namespace {

constexpr int kModelCount = 5;

// The polyphonic version normalised its mix by four, so one note sat at a
// quarter of the level knob. Mono keeps that trim, or every patch would be
// 12 dB louder than it was.
constexpr float kMixTrim = 0.25f;
constexpr int kMaxPeriod = 1024;      // 47 Hz at 48 kHz
constexpr float kFs = 48000.f;
constexpr float kPi = 3.14159265358979f;
constexpr float kDcCoef = 6.2831853f * 12.f / 48000.f;  // 12 Hz
// How fast the bow leaves when the key does. Not a musical envelope: the amp EG
// of the synth is the envelope, this only has to stop the unit — about -60 dB in
// 28 ms, short enough that no release setting has to fight it.
constexpr float kReleaseTau = 0.004f;

// How fast the loop follows the pitch the synth reports. A bend arrives as one
// value per block, and moving the period a whole sample at a block edge clicks;
// gliding the period per sample is what a string does when it is bent. 2 ms is
// short enough that a bend feels immediate, long enough that the block edges
// disappear. A note that is not bent has nothing to do: the strike already set
// exactly this period.
constexpr float kGlideCoef = 0.01f;
constexpr float kGlideDone = 0.002f;  // a thousandth of a sample: inaudible

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

const Model kModels[kModelCount] = {
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
  float period;     // the whole period, len + frac, as the pitch tracking moves it
  float lp;         // loop lowpass state
  float dc;         // the loop's own DC, taken out at 12 Hz
  float body1_y1, body1_y2;
  float body2_y1, body2_y2;
  float env;        // bow pressure, 0..1
  float env_coef;   // how fast it comes and goes
  bool  held;
  int   bow_pos;   // where the bow is along its own cycle
};

struct State {
  Voice voice;
  int   model;
  float pressure;   // 0..1, the bow, as the loop sees it
  float pressure_base;  // the knob, before the LFO reaches it
  float rosin;      // 0..1
  float bright;     // 0..1
  float level;
  float attack;     // 0..1, how long the bow takes to catch
  float tune;
  float rel_coef;   // per sample fall of the bow when the key comes up
  float noise;      // the bow's own noise, one state
  float lfo;
  float target_period;  // where the block's pitch wants the loop to be
  uint16_t last_pitch;  // the pitch that target was computed from
  bool pitch_dirty;     // the tune knob moved, so it has to be computed again

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

  // Both resonators are below a kilohertz in every model, so neither angle needs
  // the clamp the first version carried, and the second is just 2.7 times the
  // first.
  const float r1 = 0.995f - 0.02f * (1.f - m.body_q * 0.2f);
  const float w1 = 2.f * kPi * m.body_hz / kFs;
  s.b1_a1 = 2.f * r1 * arc_cosf(w1);
  s.b1_a2 = -r1 * r1;

  const float r2 = 0.993f;
  s.b2_a1 = 2.f * r2 * arc_cosf(2.7f * w1);
  s.b2_a2 = -r2 * r2;
}

// Everything but the delay line: the line is written whole by strike(), so
// zeroing it here as well meant two passes over 4 KB per note.
void resetVoice(Voice &v) {
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
  v.period = 100.f;
}

// The period of a note, in samples. Both the strike and the pitch tracking go
// through this, so a note that is not bent sits bit for bit on the period the
// strike set and nothing moves.
inline float periodForNote(float n) {
  float period = kFs / (440.f * arc_pow2f((n - 69.f) * (1.f / 12.f)));
  if (period > (float)(kMaxPeriod - 2))
    period = (float)(kMaxPeriod - 2);
  if (period < 2.f)
    period = 2.f;
  return period;
}

void setPeriod(Voice &v, float period) {
  v.period = period;
  v.len = (int)period;
  v.frac = period - (float)v.len;
}

// Reads one period back with a linear interpolation, so a note is in tune
// between the integer delay lengths.
inline float loopRead(const Voice &v) {
  int p = v.pos - v.len;
  if (p < 0)
    p += kMaxPeriod;
  const int q = p ? p - 1 : kMaxPeriod - 1;
  return v.buf[p] + (v.buf[q] - v.buf[p]) * v.frac;
}

void strike(int note, float fine) {
  Voice &v = s.voice;
  resetVoice(v);
  const float n = (float)note + fine + s.tune;
  const float period = periodForNote(n);
  setPeriod(v, period);
  // The target is where the loop already is, or the first block would glide the
  // note towards whatever the last one was.
  s.target_period = period;
  s.last_pitch = (uint16_t)(note << 8);
  s.pitch_dirty = false;
  v.held = true;
  // The bow does not catch the string instantly: at a low pressure it takes
  // longer, which is most of what a slow bow attack sounds like.
  const float t = 0.004f + 0.35f * s.attack * (1.2f - s.pressure);
  v.env_coef = 1.f - arc_pow2f(-1.f / (t * kFs) * 6.9f);
  v.env = 0.f;
  // A little something in the loop to start it, or a silent loop stays silent.
  // The whole ring is seeded, not only the first period: the loop reads a window
  // of `len` samples, and it turns out the attack is not the same if the rest of
  // the line holds an older note instead of noise (measured: 6 dB, and 34 dB in
  // the quietest band). The seeding is 1.3 us per note and that is the price of
  // the sound.
  for (int i = 0; i < kMaxPeriod; ++i)
    v.buf[i] = 0.002f * bowNoise();
}

}  // namespace

void OSC_INIT(uint32_t platform, uint32_t api) {
  program_start::reset();
  (void)platform;
  (void)api;
  s.model = 0;
  s.pressure = 0.55f;
  s.rosin = 0.5f;
  s.bright = 0.55f;
  s.level = 0.5f;
  s.attack = 0.35f;
  s.tune = 0.f;
  s.last_pitch = 0xFFFFu;
  s.pitch_dirty = true;
  s.target_period = 100.f;
  s.noise = 0.f;
  s.pressure_base = 0.55f;
  s.lfo = 0.f;
  s.rel_coef = 1.f - arc_pow2f(-1.f / (kReleaseTau * kFs) * 6.9f);
  updateBody();
  resetVoice(s.voice);
}

void OSC_CYCLE(const user_osc_param_t *const params, int32_t *yn,
               const uint32_t frames) {
  // The synth's LFO arrives as an offset on the shape parameter, which here is
  // the bow pressure: the note leans into the string and lets go.
  s.lfo += (q31_to_f32(params->shape_lfo) - s.lfo) * 0.10f;
  // One pressure, one place: the knob (A, or the Bow parameter — they are the
  // same control on two pages) plus what the LFO is doing to it. The old version
  // kept the knob's value, the smoothed value and the applied value apart, which
  // let a Bow move be cancelled by the next LFO update.
  s.pressure = clipminmaxf(0.f, s.pressure_base + 0.4f * s.lfo, 1.f);

  const Model &m = kModels[s.model];
  const float loop_gain = m.loss * (0.99985f + 0.00015f * s.pressure);
  const float damp = clipminmaxf(0.f, m.damp * (0.35f + 0.9f * s.bright), 0.995f);
  const float rosin = m.rosin * s.rosin;
  // The bow speed: pressure is both how hard it presses and how fast it drags.
  const float drive = 0.05f + 0.5f * s.pressure;

  q31_t *__restrict y = (q31_t *)yn;

  // One voice, so this loop is the whole unit. A note that is not held and whose
  // bow has left writes nothing, and the output stays at zero.
  Voice &v = s.voice;

  // The pitch the synth reports is the current one, not the one the note was
  // struck at: a bend, a glide from the arpeggiator and the tune knob all arrive
  // here, a block at a time. An untuned fork does not need to move at all, so the
  // per sample step below is skipped unless the target has left the loop behind.
  // Only when it moved: an `arc_pow2f` every block for a note nobody is bending
  // is 5% of this unit, and the compare is one instruction.
  if (params->pitch != s.last_pitch || s.pitch_dirty) {
    s.last_pitch = params->pitch;
    s.pitch_dirty = false;
    const int note_id = (int)(params->pitch >> 8);
    const float fine = (float)(params->pitch & 0xFFu) * (1.f / 255.f);
    s.target_period = periodForNote((float)note_id + fine + s.tune);
  }
  bool gliding = fabsf(s.target_period - v.period) > kGlideDone;

  for (uint32_t i = 0; i < frames; ++i) {
    float mix = 0.f;

    if (v.held || v.env >= 1e-4f) {

      // Retune the loop the way a string is retuned: the read pointer slides by
      // a fraction of a sample per sample, not a whole sample at a block edge.
      if (gliding) {
        const float d = s.target_period - v.period;
        if (fabsf(d) <= kGlideDone) {
          v.period = s.target_period;
          gliding = false;
        } else {
          v.period += d * kGlideCoef;
        }
        const int len = (int)v.period;
        v.len = len;
        v.frac = v.period - (float)len;
      }

      // The bow: pressure comes up over the attack, and when the key comes up
      // the bow leaves at once. The loop would hold the note for seconds, but
      // the amp EG of the synth comes after this slot and owns the tail.
      const float target = v.held ? 1.f : 0.f;
      v.env += (target - v.env) * (v.held ? v.env_coef : s.rel_coef);

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

    mix *= s.level * kMixTrim;
    y[i] = f32_to_q31(clipminmaxf(-1.f, osc_softclipf(0.18f, mix), 1.f));
  }
}

void OSC_NOTEON(const user_osc_param_t *const params) {
  const int note = (int)(params->pitch >> 8);
  const float fine = (float)(params->pitch & 0xFFu) * (1.f / 255.f);
  strike(note, fine);
}

// One note, so a release always ends the note that is sounding; on a mono synth
// the key that comes up is that key. The bow leaves immediately: the tail
// belongs to the amp EG.
void OSC_NOTEOFF(const user_osc_param_t *const params) {
  (void)params;
  s.voice.held = false;
}

void OSC_MUTE(const user_osc_param_t *const params) {
  (void)params;
  resetVoice(s.voice);
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
    s.pressure_base = param_val_to_f32(value);
    s.pressure = clipminmaxf(0.f, s.pressure_base + 0.4f * s.lfo, 1.f);
    break;

  case k_user_osc_param_shiftshape:
    s.bright = param_val_to_f32(value);
    break;

  case k_user_osc_param_id1: {
    int model = (int)value;
    if (model >= kModelCount)
      model = kModelCount - 1;
    if (model != s.model) {
      s.model = model;
      updateBody();
    }
    break;
  }

  case k_user_osc_param_id2:  // bow pressure, 0 .. 100
    s.pressure_base = clipminmaxf(0.f, (float)value * 0.01f, 1.f);
    s.pressure = clipminmaxf(0.f, s.pressure_base + 0.4f * s.lfo, 1.f);
    break;

  case k_user_osc_param_id3:  // rosin, 0 .. 100
    s.rosin = clipminmaxf(0.f, (float)value * 0.01f, 1.f);
    break;

  case k_user_osc_param_id4:  // bipolar percent, 100 is the centre
    s.tune = ((float)value - 100.f) * 0.24f;
    s.pitch_dirty = true;  // the glide picks it up on the next block
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
