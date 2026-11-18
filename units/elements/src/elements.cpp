/*
 * elements.cpp -- geometry morphing resonator for the KORG Nu:Tekt NTS-1.
 *
 * BSD 3-Clause License, see LICENSE.
 *
 * The sibling of reso, with a different idea behind it: instead of one fixed
 * set of modes, the mode ratios themselves are morphed while you play. One end
 * of the morph is a stiff string (harmonic series with the inharmonic stretch a
 * real string has), then the Bessel ratios of a circular membrane, then the
 * modes of a flat plate, and finally the inharmonic series of a bell. Decay and
 * damping follow the geometry, so a string is short and bright and a bell rings
 * long and dark, which is what those objects do. The exciter is filtered along
 * the way as well, the way a stiff mallet excites a string differently than a
 * soft one excites a plate.
 *
 * It is an independent implementation, inspired by the resonators of Mutable
 * Instruments Elements rather than a port of it: the NTS-1 gives a modulation
 * effect two knobs and no note information, so pitch is set by hand here.
 *
 * The instrument is mono, so there is one bank of modes and one signal: the
 * modfx slot hands over a main and a sub buffer, but an instrument with one
 * oscillator has one signal, and the SDK lets a unit on a platform without a
 * sub timbre ignore the sub buffers. A separate sub output gets the same
 * samples. The bank is stepped two samples at a time, one mode's coefficients
 * and states used for the pair.
 *
 * A-knob (TIME)   fundamental, 40 Hz .. 2 kHz
 * B-knob (DEPTH)  geometry: string -> membrane -> plate -> bell
 */

#include "usermodfx.h"
#include "../../common/program_start.h"

namespace {

constexpr int kModes = 16;
constexpr float kFs = 48000.f;

// The bank normally sums to well below one; anything past this is a runaway.
constexpr float kBankLimit = 100.f;

// The four geometries are single points on a continuum and a knob has no
// detents, so landing on the membrane by hand is luck. Within 3% of the way
// between two of them — ten counts of the knob — the knob snaps to the model
// and the object is exact: string, membrane, plate, bell. Outside that band
// nothing changes, the morph is what it was, and the bands are far apart they
// never meet. Three percent of the knob's travel would swallow a quarter of it;
// the measurement (tools/levels.cpp, one plucked note) put the step at such an
// edge at up to 7.7 dB, while these ten counts cost about a decibel.
constexpr float kSnap = 0.01f;

inline float snapGeometry(float v) {
  for (int i = 0; i <= 3; ++i) {
    const float p = (float)i * (1.f / 3.f);
    const float d = v - p;
    if (d <= kSnap && d >= -kSnap)
      return p;
  }
  return v;
}

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

// Square root without the maths library: the integer trick seeds it within a
// few percent and three Newton steps take it to float precision. Only called
// when the geometry changes, so the divisions do not matter.
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

struct Channel {
  float y1[kModes];
  float y2[kModes];
  float hp_x;
  float hp_y;
  float lp;
};

struct State {
  float pitch;     // 0..1
  float geometry;  // 0..1
  float b0[kModes];
  float a1[kModes];
  float a2[kModes];
  float exc_coef;  // exciter low pass
  float out_gain;
  Channel ch;
  int active_modes;  // modes below Nyquist, the rest is silent
};

State s;

// The ratio of mode k in one geometry. The string's comes out of the stiffness
// formula when the geometry moves; keeping all four sets — four times sixteen
// floats — for a knob movement was 256 bytes of the slot.
inline float ratioFor(int set, int k) {
  const float n = (float)(k + 1);
  switch (set) {
  case 0:
    // A real string is stiff: f_n = n * f0 * sqrt(1 + B n^2), which stretches
    // the partials apart as they go up.
    return n * sqrtf_fast(1.f + 0.0008f * n * n);
  case 1:
    return kMembrane[k];
  case 2:
    return kPlate[k];
  default:
    return kBell[k];
  }
}

void clearChannel(Channel &c) {
  for (int i = 0; i < kModes; ++i) {
    c.y1[i] = 0.f;
    c.y2[i] = 0.f;
  }
  c.hp_x = 0.f;
  c.hp_y = 0.f;
  c.lp = 0.f;
}

void updateCoefficients() {
  const float g = s.geometry * 3.f;  // 0 string .. 3 bell
  int i0 = (int)g;
  if (i0 > 2)
    i0 = 2;
  const float t = g - (float)i0;

  const float f0 = 40.f * pow2f(5.644f * s.pitch);   // 40 Hz .. 2 kHz
  // Decay time constant of the fundamental. The range is wide on purpose: the
  // string end damps away in a third of a second, the bell end rings for a
  // couple of seconds, and damping shortens the higher modes on top of that.
  //
  // The bell end used to be three minutes, which is not a bell but a note with
  // no end in sight: a modulation effect sits after the amp EG of the synth, so
  // whatever is left in the bank is heard after the key is gone, and opening
  // DEPTH left a note hanging with nothing downstream to stop it. A held note
  // still fills the bank continuously, which is all the pad at this end needs.
  const float tau = 0.35f * pow2f(0.84f * g);
  const float damp = 0.30f - 0.05f * g;              // harder geometries ring longer
  // A mode stores about (1-r^2) of what is fed into it, so a shorter ring is
  // also a louder one: the shorter tau above made the unit 19 dB louder on its
  // own. The input gain is scaled with the decay time, with 7.92 s — the ring
  // time in the middle of the depth range — as the reference, which puts the
  // loudness back where it was and leaves DEPTH a knob about the tail.
  const float excite_gain = tau * (1.f / 7.92f);

  int active = 0;

  for (int k = 0; k < kModes; ++k) {
    const float ra = ratioFor(i0, k);
    const float rb = ratioFor(i0 + 1, k);
    const float f = f0 * (ra + (rb - ra) * t);

    if (f >= 0.45f * kFs) {
      s.b0[k] = 0.f;
      s.a1[k] = 0.f;
      s.a2[k] = 0.f;
      continue;
    }

    // Every mode set is sorted by frequency, so the ones past this point are
    // all above Nyquist: the sample loop can stop here instead of running the
    // tail of the bank for nothing. It matters most at the top of the range,
    // where a bell keeps two or three modes.
    active = k + 1;

    const float tn = tau * pow2f(-damp * (float)k);
    const float r = expNegSmall(1.f / (tn * kFs));

    s.a1[k] = 2.f * r * fx_cosf(clipminmaxf(0.f, f / kFs, 0.99f));
    s.a2[k] = -r * r;
    s.b0[k] = 0.5f * excite_gain * (1.f - r * r) / (1.f + 0.5f * (float)k);
  }

  s.active_modes = active;

  // A stiff mallet on a string is brighter than a soft one on a plate.
  const float exc_fc = 700.f * pow2f(1.6f * (3.f - g));
  s.exc_coef = 1.f - expNegSmall(6.2831853f * clipminmaxf(200.f, exc_fc, 12000.f) / kFs);
  // Unity: a modulation effect should not change the level of what passes
  // through it. At the middle of the knob this bank was 10 dB below the note it
  // was handed, which is what the 18.6 is: the measured gain, with the same
  // 6 * (1 + 2g) shape over the geometry. tools/levels.cpp --pluck.
  s.out_gain = 18.6f * (1.f + 2.f * g);
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

// Digital silence in, and nothing left in the bank: every term of the sum is
// zero, so the output is zero and the work can be skipped. Not an
// approximation, and on the instrument the second timbre is silent for most of
// the time, which makes this the difference between one bank and two.
bool idleChannel(const float *xn, uint32_t frames, Channel &c) {
  const bool down = settle(c);
  if (!down)
    return false;
  for (uint32_t i = 0; i < frames; ++i)
    if (xn[i] != 0.f)
      return false;
  return true;
}

// What a sample is worth right after the bank was cleared: every mode's state
// is zero, so its output is b[n] times the exciter alone. Only the runaway
// guard needs it, and it keeps the dropped sample exactly what the old
// sample-by-sample guard produced.
float clearedSample(const float *b0, int modes, float exc, float out_gain) {
  float acc = 0.f;
  for (int n = 0; n < modes; ++n)
    acc += b0[n] * exc;
  return fx_softclipf(0.15f, acc * out_gain);
}

// Two samples per step. The bank's state is read and written once per pair
// instead of once per sample, and the two samples are two independent chains
// for the core to interleave. The second sample of a pair takes the first one's
// result as its previous state, in the same order as before, so every value is
// the same float it was.
void processMono(const float *xn, float *yn, uint32_t frames, Channel &c) {
  const int modes = s.active_modes;
  const float *const b0 = s.b0;
  const float *const a1 = s.a1;
  const float *const a2 = s.a2;
  const float exc_coef = s.exc_coef;
  const float out_gain = s.out_gain;

  settle(c);

  uint32_t i = 0;
  for (; i + 1 < frames; i += 2) {
    // Remove rumble, then shape the exciter.
    c.hp_y = 0.995f * (c.hp_y + xn[i] - c.hp_x);
    c.hp_x = xn[i];
    c.lp += (c.hp_y - c.lp) * exc_coef;
    const float e0 = c.lp;

    c.hp_y = 0.995f * (c.hp_y + xn[i + 1] - c.hp_x);
    c.hp_x = xn[i + 1];
    c.lp += (c.hp_y - c.lp) * exc_coef;
    const float e1 = c.lp;

    float acc0 = 0.f;
    float acc1 = 0.f;

    for (int n = 0; n < modes; ++n) {
      const float b = b0[n];
      const float p = a1[n];
      const float q = a2[n];
      const float y1 = c.y1[n];
      const float y2 = c.y2[n];
      const float v0 = b * e0 + p * y1 + q * y2;
      const float v1 = b * e1 + p * v0 + q * y1;
      c.y2[n] = v0;
      c.y1[n] = v1;
      acc0 += v0;
      acc1 += v1;
    }

    // Moving the geometry or the pitch jumps the modes under the ringing
    // state, and a high bank can then grow by a factor of a few per sample.
    // Legitimate audio never gets near this limit, so a runaway bank is simply
    // dropped rather than allowed to put a NaN into the slot.
    if (!(acc0 > -kBankLimit && acc0 < kBankLimit)) {
      // The second sample of the pair was stepped under the runaway: after the
      // clear it starts from an empty bank, which is one value per mode.
      clearChannel(c);
      for (int n = 0; n < modes; ++n) {
        const float v = b0[n] * e1;
        c.y1[n] = v;
        c.y2[n] = 0.f;
      }
      yn[i] = 0.f;
      yn[i + 1] = clearedSample(b0, modes, e1, out_gain);
      continue;
    }
    if (!(acc1 > -kBankLimit && acc1 < kBankLimit)) {
      clearChannel(c);
      acc1 = 0.f;
    }

    yn[i] = fx_softclipf(0.15f, acc0 * out_gain);
    yn[i + 1] = fx_softclipf(0.15f, acc1 * out_gain);
  }

  if (i < frames) {
    const float raw = xn[i];
    c.hp_y = 0.995f * (c.hp_y + raw - c.hp_x);
    c.hp_x = raw;
    c.lp += (c.hp_y - c.lp) * exc_coef;

    const float exc = c.lp;
    float acc = 0.f;
    for (int n = 0; n < modes; ++n) {
      const float y = b0[n] * exc + a1[n] * c.y1[n] + a2[n] * c.y2[n];
      c.y2[n] = c.y1[n];
      c.y1[n] = y;
      acc += y;
    }
    if (!(acc > -kBankLimit && acc < kBankLimit)) {
      clearChannel(c);
      acc = 0.f;
    }
    yn[i] = fx_softclipf(0.15f, acc * out_gain);
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

  s.pitch = 0.45f;
  s.geometry = 0.35f;

  updateCoefficients();
  clearChannel(s.ch);
}

void MODFX_PROCESS(const float *main_xn, float *main_yn, const float *sub_xn,
                   float *sub_yn, uint32_t frames) {
  // The NTS-1 has one oscillator, so there is no second timbre: the sub buffers
  // carry the same signal, and on a platform without a sub timbre the SDK says
  // they can be ignored. One bank renders the one signal, and a host that still
  // hands over a separate sub output gets those same samples.
  (void)sub_xn;
  const bool second_out = sub_yn != main_yn;

  // A channel handed digital silence with an empty bank has nothing to compute:
  // every term of its sum is zero, so its output is written and the DSP is
  // skipped, which is most of what the slot is handed between notes.
  if (idleChannel(main_xn, frames, s.ch)) {
    for (uint32_t i = 0; i < frames; ++i)
      main_yn[i] = 0.f;
  } else {
    processMono(main_xn, main_yn, frames, s.ch);
  }

  if (second_out)
    for (uint32_t i = 0; i < frames; ++i)
      sub_yn[i] = main_yn[i];
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
    s.pitch = v;
    break;

  case k_user_modfx_param_depth:
    s.geometry = snapGeometry(v);
    break;

  default:
    return;
  }

  updateCoefficients();
}

void MODFX_SUSPEND(void) {
  clearChannel(s.ch);
}

void MODFX_RESUME(void) {}
