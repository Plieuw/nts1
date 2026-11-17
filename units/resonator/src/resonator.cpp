/*
 * resonator.cpp -- multi model modal resonator for the KORG Nu:Tekt NTS-1.
 *
 * BSD 3-Clause License, see LICENSE.
 *
 * The modulation slot has `elements`, which is the same idea with the pitch set
 * by hand, because a modulation effect gets two knobs and no note. This is the
 * same engine in the oscillator slot, where the note number is available: 17
 * models, four voices, played from the keyboard.
 *
 * The models are the objects the other units imitate. A model is a ratio set
 * plus the way that object behaves under the knobs: how long it rings, how fast
 * its high modes go, and how much of the strike lands up there. Seventeen, not
 * twenty-three: every model that measured too close to another one is gone, so
 * that switching the model knob always changes the object and not just its
 * brightness.
 *
 *   0  string     harmonic series with the inharmonic stretch a stiff string has
 *   1  membrane   Bessel ratios of a circular membrane: drum heads
 *   2  bell       strongly inharmonic partials, the high ones carry the character
 *   3  kalimba    free-free tine, almost all of its energy in the first modes
 *   4  glass      circular plate with a free edge: glass harp, cymbal
 *   5  marimba    tuned bar, 1:3.9:9.2 with the n^2 tail of a free-free bar
 *   6  xylophone  undercut bar, 1:3:6, brittle and short
 *   7  vibraphone bar with an octave of detuned pairs: the shimmer of the bars
 *   8  piano      stiff string with its unisons detuned, hammered not plucked
 *   9  steel drum octave led pan: a low note with the octave blooming over it
 *  10  gong       dense inharmonic cluster, beating, the highs barely die
 *  11  bowl       singing bowl: a few widely spaced modes that ring for ever
 *  12  triangle   metal with almost nothing but high partials, very long
 *  13  woodblock  two or three modes and a knock, gone in a fifth of a second
 *  14  log drum   slit drum: strong fundamental, hollow 1:2.75 ratio, short
 *  15  tabla      head loaded so it is tuned, near harmonic, with a snap
 *  16  reed       odd harmonics of a closed pipe with a reed: hollow, nasal
 *
 * A note is struck: a burst of filtered noise goes into the bank, the mode
 * levels depend on where the strike lands (a real bar has nodes that a strike
 * can miss), and the modes ring out with the high ones dying faster than the
 * low ones unless the brightness knob says otherwise.
 *
 * Four voices, because the oscillator API hands out note-on and note-off: a new
 * note takes a free voice, a note that is still ringing is not cut off by the
 * next one, and the fifth note steals the oldest. A voice keeps the pitch it was
 * struck at — a struck object does not glide — except for the newest one, which
 * follows the keyboard so a solo line still bends.
 *
 * A-knob (SHAPE)   decay, 0.1 s .. 12 s
 * B-knob (SHIFT)   brightness: how long the high modes live, and how open the
 *                  strike sounds
 * Edit 1 Model     0 .. 22, see above
 * Edit 2 Strike    soft mallet .. hard hammer
 * Edit 3 Pos       where the strike lands, 0 .. 1 across the object
 * Edit 4 Tune      +- 24 semitones
 * Edit 5 Level
 * Edit 6 Damp      extra damping of the high modes
 */

#include "userosc.h"

namespace {

constexpr int kModes = 16;
constexpr int kModels = 17;
constexpr int kVoices = 4;
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

inline float expNegSmall(float a) {
  return 1.f - a + a * a * (0.5f - a * 0.16666667f);
}

// The ratio sets. A model is a set of modes plus the way that object behaves
// under the knobs; together they are the palette of the unit. Each one is an
// instrument a bank of modes can actually tell apart: measured against each
// other, every model here is at least four standard deviations of its spectral
// envelope away from its closest neighbour (see the README), which is why there
// is no second harmonic tube next to the string and no second bell.

// Circular membrane: ratios of the Bessel zeros, 1.000 being the fundamental.
static const float kMembrane[kModes] = {
    1.000f, 1.593f, 2.135f, 2.295f, 2.653f, 2.917f, 3.155f, 3.500f,
    3.598f, 3.647f, 4.059f, 4.230f, 4.831f, 5.132f, 5.428f, 5.467f};

// Bell: strongly inharmonic, the high partials carry a lot of the character.
static const float kBell[kModes] = {
    1.000f, 2.000f, 2.400f, 3.000f, 3.900f, 4.600f, 5.400f, 6.800f,
    7.900f, 9.200f, 10.60f, 12.10f, 13.70f, 15.40f, 17.20f, 19.10f};

// Kalimba: a free-free tine. Nearly everything is in the first partial and the
// series runs away fast, which is what makes it read as a tine and not a string.
static const float kKalimba[kModes] = {
    1.000f, 6.270f, 17.55f, 34.40f, 45.00f, 60.00f, 75.00f, 90.00f,
    105.0f, 120.0f, 135.0f, 150.0f, 165.0f, 180.0f, 195.0f, 210.0f};

// Circular plate with a free edge: handpan, glass harp, and the gong behind both.
static const float kGlass[kModes] = {
    1.000f, 2.320f, 4.250f, 6.630f, 9.380f, 12.50f, 15.90f, 19.60f,
    23.60f, 27.90f, 32.40f, 37.20f, 42.20f, 47.40f, 52.90f, 58.60f};

// Marimba: a bar tuned so its first three modes land on 1, 4 and 9-ish, with the
// free-free bar series running up from there. Softer and lower than a xylophone.
static const float kMarimba[kModes] = {
    1.000f, 3.930f, 9.240f, 16.70f, 26.00f, 37.70f, 51.80f, 68.30f,
    87.20f, 108.0f, 132.0f, 158.0f, 187.0f, 218.0f, 252.0f, 289.0f};

// Xylophone: the same bar undercut so the modes sit at 1, 3 and 6, which is why
// it reads as wood and not as a marimba.
static const float kXylophone[kModes] = {
    1.000f, 2.980f, 5.960f, 9.940f, 14.90f, 20.90f, 27.80f, 35.80f,
    44.70f, 54.70f, 65.60f, 77.60f, 90.50f, 104.0f, 119.0f, 135.0f};

// Vibraphone: the bar series again, but the pair of bars under each note is
// tuned a few cents apart. The two almost coincide, and two modes a third of a
// percent apart are what a vibraphone sounds like with its motor running.
static const float kVibraphone[kModes] = {
    1.000f, 1.004f, 3.980f, 3.994f, 9.900f, 9.935f, 17.60f, 17.66f,
    27.20f, 27.30f, 38.50f, 38.63f, 51.50f, 51.68f, 66.20f, 66.43f};

// Piano: a stiff string, stretched less than the plain string model, with the
// two or three unisons of a real note detuned by a cent or so. That slow beat is
// most of what makes a hammered string sound like a piano and not a harp.
static const float kPiano[kModes] = {
    1.0000f, 1.0015f, 2.0008f, 2.0038f, 3.0027f, 3.0072f, 4.0064f, 4.0125f,
    5.0125f, 5.0200f, 6.0216f, 6.0307f, 7.0343f, 7.0449f, 8.0512f, 8.0634f};

// Steel drum: a pan is tuned in octaves, the second partial is as strong as the
// first and everything above it doubles again, which is the bloom it has.
static const float kSteelDrum[kModes] = {
    1.000f, 2.000f, 3.000f, 4.000f, 5.000f, 6.000f, 8.000f, 10.00f,
    12.00f, 14.00f, 16.00f, 18.00f, 20.00f, 22.00f, 24.00f, 26.00f};

// Gong: a dense inharmonic cluster with the low modes doubled a hair apart, so
// the whole thing beats against itself. The high modes outlive the low ones,
// which is the opposite of every struck wooden thing here.
static const float kGong[kModes] = {
    1.000f, 1.470f, 1.474f, 2.090f, 2.560f, 2.566f, 3.080f, 3.640f,
    4.150f, 4.700f, 5.400f, 6.100f, 6.900f, 7.800f, 8.800f, 9.900f};

// Singing bowl: a few widely spaced modes, each one long enough to hear on its
// own, with the pair on the fundamental giving it its slow wobble.
static const float kBowl[kModes] = {
    1.000f, 2.700f, 2.706f, 5.200f, 8.100f, 11.50f, 15.40f, 19.80f,
    24.70f, 30.00f, 35.80f, 42.00f, 48.70f, 55.80f, 63.40f, 71.40f};

// Triangle: bent steel rod, so the fundamental is weak and the character lives
// in the partials from the third up, which sit far apart and ring for seconds.
static const float kTriangle[kModes] = {
    1.000f, 2.400f, 3.900f, 5.800f, 8.000f, 10.40f, 13.10f, 15.90f,
    19.00f, 22.30f, 25.80f, 29.50f, 33.40f, 37.50f, 41.80f, 46.30f};

// Woodblock: a hollow block of wood with two or three modes and nothing else —
// the gaps between the ratios are as much of the sound as the modes themselves.
static const float kWoodblock[kModes] = {
    1.000f, 2.400f, 4.900f, 7.600f, 10.90f, 14.40f, 18.40f, 22.60f,
    27.20f, 32.10f, 37.30f, 42.80f, 48.60f, 54.70f, 61.10f, 67.80f};

// Log drum: a slit cut in a log, one strong low mode and a hollow 1:2.75 above
// it, the pattern the whole family of slit drums and tongue drums shares.
static const float kLogDrum[kModes] = {
    1.000f, 2.750f, 5.400f, 8.900f, 13.30f, 18.50f, 24.50f, 31.40f,
    39.10f, 47.60f, 57.00f, 67.20f, 78.30f, 90.20f, 103.0f, 117.0f};

// Tabla: the black patch loads the head until it is tuned, so a drum that would
// otherwise be a membrane turns almost harmonic. Short, with a snap and a bend
// of pitch we cannot do here, but the tuning is most of the recognition.
static const float kTabla[kModes] = {
    1.000f, 2.000f, 3.000f, 4.020f, 5.000f, 6.000f, 7.000f, 8.020f,
    9.000f, 10.00f, 11.00f, 12.00f, 13.00f, 14.00f, 15.00f, 16.00f};

// Reed: a stopped pipe with a reed, odd harmonics only, the fifth strong. The
// hollowness is the gap where every even harmonic should have been.
static const float kReed[kModes] = {
    1.000f, 3.000f, 5.000f, 7.000f, 9.000f, 11.00f, 13.00f, 15.00f,
    17.00f, 19.00f, 21.00f, 23.00f, 25.00f, 27.00f, 29.00f, 31.00f};

// String: a real string is stiff, f_n = n * f0 * sqrt(1 + B n^2), which
// stretches the partials apart as they go up. f_n = n * f0 * sqrt(1 + B n^2), which stretches the
// partials apart as they go up. B = 0.0008, written out.
static const float kString[kModes] = {
    1.000f, 2.003f, 3.011f, 4.026f, 5.050f, 6.086f, 7.136f, 8.202f,
    9.287f, 10.39f, 11.52f, 12.67f, 13.85f, 15.06f, 16.29f, 17.56f};

// How a model behaves under the knobs, on top of the shared envelope: decay
// scales the A knob's time, spread scales how much faster the high modes die
// than the low ones, and tilt says how much of the strike reaches the top.
//
// gain, reg and bend are the level fit. gain trims the model to the same loudness
// as the others at middle C. reg and bend correct what the model does over the
// rest of the keyboard: the shared curve lifts the high notes by a fixed amount
// per octave, but a bank that keeps its modes up high (a xylophone) swells where
// one that runs out of modes (a piano) falls away, so each model gets its own
// slope (reg) and curvature (bend), in octaves from middle C, measured over C1
// to C7 and fitted through the note the gain was measured at.
struct ModelSpec {
  const float *ratio;
  float decay;
  float spread;
  float tilt;
  float gain;
  float reg;
  float bend;
};

// Ordered like the model list at the top of the file.
static const ModelSpec kSpec[kModels] = {
    {kString, 1.00f, 1.00f, 0.50f, 1.000f, +0.184f, +0.019f},  // 0 string
    {kMembrane, 1.00f, 1.00f, 0.50f, 1.000f, +0.200f, +0.032f},  // 1 membrane
    {kBell, 1.00f, 1.00f, 0.50f, 1.000f, +0.187f, +0.020f},  // 2 bell
    {kKalimba, 1.00f, 1.00f, 0.50f, 1.000f, -0.078f, +0.004f},  // 3 kalimba
    {kGlass, 1.00f, 1.00f, 0.50f, 1.000f, +0.097f, -0.024f},  // 4 glass
    {kMarimba, 1.30f, 0.90f, 0.45f, 0.907f, -0.024f, -0.001f},  // 5 marimba
    {kXylophone, 0.55f, 1.15f, 0.28f, 1.680f, +0.001f, -0.025f},  // 6 xylophone
    {kVibraphone, 2.40f, 0.55f, 0.55f, 0.791f, +0.207f, -0.009f},  // 7 vibraphone
    {kPiano, 2.00f, 0.85f, 0.40f, 0.718f, +0.296f, +0.060f},  // 8 piano
    {kSteelDrum, 1.10f, 0.60f, 0.35f, 1.103f, +0.158f, +0.007f},  // 9 steel drum
    {kGong, 3.00f, 0.30f, 0.30f, 0.909f, +0.226f, +0.046f},  // 10 gong
    {kBowl, 3.40f, 0.25f, 0.60f, 0.966f, +0.107f, -0.020f},  // 11 bowl
    {kTriangle, 2.60f, 0.70f, 0.20f, 1.548f, +0.086f, -0.035f},  // 12 triangle
    {kWoodblock, 0.22f, 1.60f, 0.60f, 1.620f, +0.084f, -0.025f},  // 13 woodblock
    {kLogDrum, 0.35f, 1.30f, 0.75f, 1.303f, +0.069f, -0.011f},  // 14 log drum
    {kTabla, 0.45f, 1.10f, 0.85f, 1.037f, +0.186f, +0.021f},  // 15 tabla
    {kReed, 0.90f, 0.90f, 0.35f, 1.524f, +0.118f, -0.017f},  // 16 reed
};
/*---------------------------------------------------------------------------*
 * One voice: a bank of modes struck once
 *---------------------------------------------------------------------------*/

struct Voice {
  // Everything that depends on the pitch this voice was struck at.
  float a1[kModes];
  float a2[kModes];
  float amp[kModes];
  float y1[kModes];
  float y2[kModes];
  float f0;      // fundamental the bank was built for, Hz
  float note;    // note number as a float, for the level curve
  float out_gain;
  int modes;     // modes below Nyquist, the rest is silent
  int note_id;   // key that started it, 0..127
  uint32_t age;  // larger is older, for stealing
  bool held;     // key still down

  // The strike.
  float exc_coef;
  float exc_env_coef;
  float exc_lp;
  float exc_env;
};

struct State {
  // User parameters.
  int model;
  float decay;    // 0..1, the A knob
  float bright;   // 0..1, the B knob
  float strike;   // 0..1
  float pos;      // 0..1
  float damp;     // 0..1
  float level;    // 0..1
  float tune;     // semitones

  Voice voice[kVoices];
  uint32_t age;
  uint32_t rng;
};

State s;

void clearBank(Voice &v) {
  for (int k = 0; k < kModes; ++k) {
    v.y1[k] = 0.f;
    v.y2[k] = 0.f;
  }
  v.exc_lp = 0.f;
  v.exc_env = 0.f;
}

void silenceAll() {
  for (int i = 0; i < kVoices; ++i) {
    clearBank(s.voice[i]);
    s.voice[i].held = false;
  }
}

// Whether a voice has anything left to say. A silent voice costs nothing: the
// render loop skips it, which is what keeps four voices affordable.
bool voiceQuiet(const Voice &v) {
  if (v.exc_env > 1e-7f)
    return false;
  float mag = 0.f;
  for (int k = 0; k < v.modes; ++k)
    mag += fabsf(v.y1[k]) + fabsf(v.y2[k]);
  return mag < 1e-9f;
}

// Everything that depends on the pitch, for one voice. Called at a strike and
// whenever a knob that shapes the bank moves.
void voiceCoefficients(Voice &v, float f0, float note) {
  const ModelSpec &spec = kSpec[s.model];
  const float *const ratio = spec.ratio;
  const float tau = 0.1f * pow2f(6.9f * s.decay) * spec.decay;   // 0.1 s .. 12 s
  const float spread =
      (0.32f - 0.28f * s.bright + 0.30f * s.damp) * spec.spread;

  v.f0 = f0;
  v.note = note;

  int active = 0;

  for (int k = 0; k < kModes; ++k) {
    const float f = f0 * ratio[k];

    if (f >= 0.45f * kFs) {
      v.amp[k] = 0.f;
      v.a1[k] = 0.f;
      v.a2[k] = 0.f;
      continue;
    }

    // Ascending ratios, so everything past this point is above Nyquist too.
    active = k + 1;

    const float tn = tau * pow2f(-spread * (float)k);
    const float r = expNegSmall(1.f / (tn * kFs));

    v.a1[k] = 2.f * r * osc_cosf(clipminmaxf(0.f, f / kFs, 0.99f));
    v.a2[k] = -r * r;

    // Where the strike lands: a mode with a node there is barely excited, which
    // is the difference between a bar struck in the middle and at the end.
    const float node =
        0.15f + 0.85f * fabsf(osc_sinf(0.5f * s.pos * (float)(k + 1)));

    // The strike goes straight into the mode, not through a coupling factor
    // that depends on how fast the mode decays: that way a short string is not
    // a hundred times louder than a long bell. The 1/(1 + k/2) tilt is what a
    // real strike injects, less energy into the high modes.
    v.amp[k] = kStrikeGain * (1.f + 0.5f * s.strike) * node /
               (1.f + spec.tilt * (float)k);
  }

  v.modes = active;

  // The strike itself: a noise burst, short and bright for a hard hammer, long
  // and dark for a soft mallet.
  const float exc_fc = 300.f * pow2f(4.9f * s.strike);   // 300 Hz .. 8.9 kHz
  v.exc_coef =
      1.f - expNegSmall(6.2831853f * clipminmaxf(120.f, exc_fc, 16000.f) / kFs);
  const float env_tau = 0.010f * pow2f(-2.0f * s.strike);  // 10 ms .. 2.5 ms
  v.exc_env_coef = expNegSmall(1.f / (env_tau * kFs));

  // Every mode starts in phase, so the attack is as loud as the sum of the
  // strike amplitudes: a low note that keeps all sixteen modes, or the multi
  // model with its dense bank, would otherwise be far louder than a high note
  // that only keeps five. Normalising that sum keeps the keyboard and the
  // models even, and the level knob then sits on top of it.
  float sum = 0.f;
  for (int k = 0; k < active; ++k)
    sum += v.amp[k];
  // Below about middle C the modes sit close enough together to add up in
  // phase, and an octave higher they scatter: measured over the keyboard that
  // is a factor of seven, which no instrument does. This curve takes most of it
  // out again without flattening the register, and the per model slope and bend
  // on top of it take out the rest: a marimba swells around middle C because its
  // bar modes keep fitting below Nyquist, a piano falls away at the top because
  // its stretched partials run out first.
  const float oct = (note - 60.f) * (1.f / 12.f);
  const float trim = clipminmaxf(
      0.05f, pow2f((0.85f + spec.reg) * oct + spec.bend * oct * oct), 32.f);
  v.out_gain = ((sum > 1e-9f) ? (kOutSum / sum) : 1.f) * trim * spec.gain;
}

float f0ForNote(int note_id, float fine, float tune) {
  const float n = clipminmaxf(0.f, (float)note_id + fine, 150.f);
  const int ni = (int)n;
  const int nf = (int)((n - (float)ni) * 255.f);
  return osc_w0f_for_note((uint8_t)ni, (uint8_t)nf) * (float)k_samplerate *
         pow2f(tune * (1.f / 12.f));
}

// Picks the voice a new note goes to: the same note first (a repeat is a new
// strike, not a second voice), then a key that has been released, then the
// oldest, and only if every voice is still held, the one that has rung the
// quietest.
int allocateVoice(int note_id) {
  for (int i = 0; i < kVoices; ++i)
    if (s.voice[i].held && s.voice[i].note_id == note_id)
      return i;

  // A key that has been released first, and of those the one that was struck
  // longest ago: a voice that has rung for a while is the least missed.
  int released = -1;
  for (int i = 0; i < kVoices; ++i) {
    if (s.voice[i].held)
      continue;
    if (released < 0 || s.voice[i].age < s.voice[released].age)
      released = i;
  }
  if (released >= 0)
    return released;

  int oldest = 0;
  for (int i = 1; i < kVoices; ++i)
    if (s.voice[i].age < s.voice[oldest].age)
      oldest = i;
  return oldest;
}

void strike(int note_id, float fine) {
  const int slot = allocateVoice(note_id);
  Voice &v = s.voice[slot];

  const float note = (float)note_id + fine;
  voiceCoefficients(v, f0ForNote(note_id, fine, s.tune), note);
  clearBank(v);
  v.note_id = note_id;
  v.age = ++s.age;
  v.held = true;
  v.exc_env = 1.f;   // the strike: one burst, from here the bank rings alone
}

void updatePitch(const user_osc_param_t *const params) {
  // Only the newest note follows the keyboard: a struck body keeps the pitch it
  // was struck at, but a solo line still glides with the tune knob and the
  // fine tune the synth sends.
  const int note_id = (int)(params->pitch >> 8);
  const float fine = (float)(params->pitch & 0xFFu) * (1.f / 255.f);

  for (int i = 0; i < kVoices; ++i) {
    Voice &v = s.voice[i];
    if (!v.held || v.note_id != note_id)
      continue;
    const float f0 = f0ForNote(note_id, fine, s.tune);
    if (fabsf(f0 - v.f0) > v.f0 * 0.003f)   // five cents is below the ear
      voiceCoefficients(v, f0, (float)note_id + fine);
  }
}

void render(q31_t *__restrict y, const uint32_t frames) {
  // How many voices are doing anything, for the mix.
  int live = 0;
  bool quiet[kVoices];
  for (int i = 0; i < kVoices; ++i) {
    quiet[i] = voiceQuiet(s.voice[i]);
    if (quiet[i]) {
      if (s.voice[i].exc_env > 0.f)
        clearBank(s.voice[i]);
    } else {
      ++live;
    }
  }
  if (live == 0) {
    for (uint32_t i = 0; i < frames; ++i)
      *y++ = 0;
    return;
  }

  // Four notes at once are not four times as loud: the mix lets a single voice
  // keep its level and tucks the others in under it.
  const float mix = s.level / (1.f + 0.35f * (float)(live - 1));

  for (uint32_t i = 0; i < frames; ++i) {
    float acc = 0.f;

    for (int vi = 0; vi < kVoices; ++vi) {
      if (quiet[vi])
        continue;
      Voice &v = s.voice[vi];

      float exc = 0.f;
      if (v.exc_env > 1e-7f) {
        uint32_t rng = s.rng;
        rng ^= rng << 13;
        rng ^= rng >> 17;
        rng ^= rng << 5;
        s.rng = rng;
        const float noise = (float)(int32_t)rng * (1.f / 2147483648.f);
        v.exc_lp += (noise - v.exc_lp) * v.exc_coef;
        exc = v.exc_lp * v.exc_env;
        v.exc_env *= v.exc_env_coef;
      } else {
        v.exc_env = 0.f;
        v.exc_lp = 0.f;
      }

      const int modes = v.modes;
      const float *const amp = v.amp;
      const float *const a1 = v.a1;
      const float *const a2 = v.a2;
      float *const y1 = v.y1;
      float *const y2 = v.y2;

      float sum = 0.f;
      for (int k = 0; k < modes; ++k) {
        const float val = amp[k] * exc + a1[k] * y1[k] + a2[k] * y2[k];
        y2[k] = y1[k];
        y1[k] = val;
        sum += val;
      }

      // A strike or a glide can move the modes under a bank that is still
      // ringing and make it run away; drop it rather than pass a NaN on.
      if (!(sum > -kBankLimit && sum < kBankLimit)) {
        clearBank(v);
        sum = 0.f;
      }

      acc += sum * v.out_gain;
    }

    *y++ = f32_to_q31(osc_softclipf(0.08f, acc * mix));
  }
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
  s.age = 0;

  s.rng = 0x9E3779B9u ^ osc_mcu_hash();
  if (s.rng == 0u)
    s.rng = 0x1234567u;

  for (int i = 0; i < kVoices; ++i) {
    Voice &v = s.voice[i];
    v.note_id = 60;
    v.age = 0;
    v.held = false;
    v.modes = 0;
    voiceCoefficients(v, 261.63f, 60.f);   // middle C, until a note arrives
    clearBank(v);
  }
}

void OSC_CYCLE(const user_osc_param_t *const params, int32_t *yn,
               const uint32_t frames) {
  updatePitch(params);
  render((q31_t *)yn, frames);
}

void OSC_NOTEON(const user_osc_param_t *const params) {
  const int note_id = (int)(params->pitch >> 8);
  const float fine = (float)(params->pitch & 0xFFu) * (1.f / 255.f);
  strike(note_id, fine);
}

void OSC_NOTEOFF(const user_osc_param_t *const params) {
  // The key comes up, the object keeps ringing: the voice is free for the next
  // note, but nothing is cut short here.
  const int note_id = (int)(params->pitch >> 8);
  for (int i = 0; i < kVoices; ++i)
    if (s.voice[i].held && s.voice[i].note_id == note_id)
      s.voice[i].held = false;
}

void OSC_MUTE(const user_osc_param_t *const params) {
  (void)params;
  silenceAll();
}

void OSC_PARAM(uint16_t index, uint16_t value) {
  bool reshape = true;

  switch (index) {
  case k_user_osc_param_shape:  // 10 bit
    s.decay = param_val_to_f32(value);
    break;

  case k_user_osc_param_shiftshape:  // 10 bit
    s.bright = param_val_to_f32(value);
    break;

  case k_user_osc_param_id1: {
    int model = (int)value;
    if (model < 0)
      model = 0;
    if (model > kModels - 1)
      model = kModels - 1;
    s.model = model;
    break;
  }

  case k_user_osc_param_id2:  // 0 .. 100
    s.strike = clipminmaxf(0.f, (float)value * 0.01f, 1.f);
    break;

  case k_user_osc_param_id3:  // 0 .. 100
    s.pos = clipminmaxf(0.f, (float)value * 0.01f, 1.f);
    break;

  case k_user_osc_param_id4:  // bipolar percent, 100 is the centre
    s.tune = ((float)value - 100.f) * 0.24f;
    break;

  case k_user_osc_param_id5:  // 0 .. 100
    s.level = clipminmaxf(0.f, (float)value * 0.01f, 1.f);
    reshape = false;
    break;

  case k_user_osc_param_id6:  // 0 .. 100
    s.damp = clipminmaxf(0.f, (float)value * 0.01f, 1.f);
    break;

  default:
    reshape = false;
    break;
  }

  // Anything that shapes the bank re-lands every voice that is still ringing:
  // the notes keep their pitch and the object changes under them.
  if (!reshape)
    return;
  for (int i = 0; i < kVoices; ++i) {
    Voice &v = s.voice[i];
    if (v.modes == 0)
      continue;
    voiceCoefficients(v, v.f0, v.note);
  }
}
