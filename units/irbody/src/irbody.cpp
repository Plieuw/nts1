/*
 * irbody.cpp -- body and space for the KORG Nu:Tekt NTS-1, built as filters.
 *
 * BSD 3-Clause License, see LICENSE.
 *
 * Runs in the REVERB slot, the last stage of the chain, so the oscillator, the
 * modulation units and the delay are all already in the sound that reaches it.
 *
 * The first version of this unit convolved the sound with a modelled impulse
 * response: 4096 taps in sixteen frequency domain partitions, 603 KB of SDRAM
 * for the spectra and a 512 point FFT every 256 samples. It sounded right and it
 * cost more than every other unit here put together. The same model can be
 * played instead of convolved, and that is what this version does: ir_model.h
 * says what a violin body or a steel plate does (a handful of resonances with
 * their own damping, a diffuse tail that gets dark as it dies, early
 * reflections), and both the old IRs and this unit are built from exactly those
 * numbers. Convolving with a response and running the network that produced it
 * are two ways to hear the same object, so this is not a new sound: it is the
 * same one with the finite response taken out of it.
 *
 *   resonances      one two pole resonator per mode, driven by the input
 *   diffuse tail    three delay lines in a feedback delay network
 *   size            the delays of that network
 *   damping         the same one pole low pass on the wet signal as before
 *
 * The sustain is the one difference worth knowing about. An 85 ms response stops
 * 85 ms after the sound that fed it; a network keeps ringing for as long as it
 * is fed, so a held note sits inside the body instead of leaving it after a few
 * frames. That is what a body does, and it is the one place where this is not
 * only cheaper but better.
 *
 * A-knob (TIME)   which body: violin, cello, piano, guitar, wood, steel,
 *                 clarinet, drum
 * B-knob (DEPTH)  damping of the wet signal, dark .. open
 * DEL + B knob    dry/wet mix
 *
 * Two engines run during a switch and their outputs are crossfaded over 85 ms,
 * so turning the knob cannot click. Everything is set up in the parameter
 * callback, never in the audio callback.
 */

#include "userrevfx.h"
#include "../../common/program_start.h"
#include "../../common/silence.h"

#include "../../common/arcmath.h"

#include "ir_model.h"

namespace {

constexpr float kFs = 48000.f;
constexpr float kPi = 3.14159265358979f;

constexpr int kMaxModes = 11;   // the widest mode set in ir_model.h
constexpr int kCombs = 3;
// The longest comb at the largest body the knob may reach: 11.9 ms * 0.78 *
// 48 kHz is 446 samples, so 448 covers every preset and buildEngine's clamp
// never bites.
constexpr int kCombMax = 448;
// A body is at most a guitar, however the model describes the object: the tail
// of a piano soundboard or a steel plate is not a room to be heard in, and the
// body this unit puts under a note should never be larger than the little
// soundbox a guitar is. Anything the model puts above these is brought back to
// the guitar's size and decay.
constexpr float kBodySize = 0.78f;   // the guitar's comb scale
constexpr float kMaxModeTau = 0.022f;  // the guitar's mode decay, seconds
constexpr float kMaxTailTau = 0.010f;  // and the decay of its diffuse tail
constexpr int kAp0Len = 240;    // 5.0 ms allpass
constexpr int kAp1Len = 96;     // 2.0 ms allpass
constexpr int kPresetCount = irmodel::kPresetCount;
constexpr int kFadeSamples = 4096;  // 85 ms crossfade on a switch
// A body is not a reverb: its own tail is 8 to 35 ms. Twenty milliseconds of
// true silence closes the gate over ten, so the slot cannot keep sounding after
// the input is gone, whatever state it was left in.
constexpr float kGateFloor = 1e-6f;
constexpr int kGateHold = 960;            // 20 ms of digital silence
constexpr float kGateStep = 1.f / 480.f;  // 10 ms ramp
// A body never sums this loud; anything past it is a runaway, not a sound.
constexpr float kBankLimit = 100.f;
// Left and right read the same three lines a few samples apart: a body is one
// object, so its modes stay in the middle, but its tail arrives at two points
// of the corpus, and that difference is what a listener hears as size.
const int kTap[kCombs] = {53, 97, 31};
constexpr float kStereoTrim = 0.72f;  // see engineSample

// The old unit normalised every response to a peak of 0.5 and convolved with it.
// A mode of amplitude A and damping 1 - r has a resonant gain of A / (2 (1 - r))
// in that response, which is what the mode gain in buildEngine puts back, so a
// violin still emphasises the notes a violin emphasises by the same amount.
//
// The four allpasses' feedback, named so the blocks below read as one shape:
// the three that smear the tail, the long one on the side signal, and the short
// one that turns the right channel's modes a little.
constexpr float kApG = 0.6f;
constexpr float kApG4 = 0.4f;

// Only the buffers that need to be big live in SDRAM: the eighty five
// milliseconds of comb line the tail stands in. Every other line here is a few
// hundred floats, and each of them is read and written once per sample, so they
// stay in SRAM where a sample does not wait for them. The .sdram section is not
// zeroed at load time, so the buffers in it are cleared explicitly in
// clearBuffers; the .bss ones are zeroed by the loader and cleared anyway.
float g_comb[2][kCombs][kCombMax] __sdram;
float g_ap0[2][kAp0Len];
float g_ap1[2][kAp1Len];
float g_ap2[2][kAp1Len];  // the side signal, so left and right differ
// The modes are one object, but they do not radiate from one point either: the
// right channel takes them through an allpass of a third of a millisecond. An
// allpass keeps the level, so this widens the upper partials of every body
// without putting a notch in the sum.
constexpr int kAp4Len = 17;
float g_ap4[2][kAp4Len];

/*---------------------------------------------------------------------------*
 * One engine: the resonances of an object plus its tail
 *---------------------------------------------------------------------------*/

struct Engine {
  // Resonances, as two pole resonators driven by the input.
  int modes;
  float a1[kMaxModes];
  float a2[kMaxModes];
  float mode_gain[kMaxModes];
  float y1[kMaxModes];
  float y2[kMaxModes];

  // Diffuse tail.
  int comb_len[kCombs];
  int comb_pos[kCombs];
  float comb_fb[kCombs];
  float comb_damp[kCombs];
  float comb_lp[kCombs];
  float comb_fb_out[kCombs];
  float tail_gain;

  // Diffusion of the tail.
  int ap0_pos;
  int ap1_pos;

  // The model's tail is a low band plus a high band that dies away faster; the
  // split is a one pole at 450 Hz on the way in.
  float tail_cut;
  float tail_hi;
  float tail_lp_in;
  float tail_out_cut;
  float tail_out_lp;
  float tail_out_lp_s;
  int ap4_pos;
  float level;
  int ap2_pos;
};

// The engines are state, not sound: 744 bytes for the pair, read and written
// every sample, so they sit in SRAM with the lines above. The section is not
// zeroed at load, so every field is written by clearEngine before it is read.
Engine g_eng[2];

struct State {
  int active;   // engine slot in the output
  int target;   // engine slot fading in, -1 when not fading
  float pos;    // where the body knob sits, 0 .. presets-1, continuous
  float queued; // last request that arrived during a fade, -1 when none
  float damp_lp_r;
  float fade;   // 0..1, position of the crossfade
  float damp;   // 0..1
  float wet;    // 0..1
  float damp_coef;
  float damp_lp;
  int ready;    // set once the first engine has been built
  float gate;   // 0..1, silence gate at the output
  int quiet;    // samples of true silence behind us
};

State s;  // read every sample: gate, damping, the fade position


void clearEngine(Engine &e) {
  for (int k = 0; k < kMaxModes; ++k) {
    e.y1[k] = 0.f;
    e.y2[k] = 0.f;
  }
  for (int c = 0; c < kCombs; ++c) {
    e.comb_lp[c] = 0.f;
    e.comb_pos[c] = 0;
  }
  e.tail_lp_in = 0.f;
  e.tail_out_lp = 0.f;
  e.tail_out_lp_s = 0.f;
  e.ap0_pos = 0;
  e.ap1_pos = 0;
  e.ap2_pos = 0;
  e.ap4_pos = 0;
}

void clearBuffers(int slot) {
  for (int c = 0; c < kCombs; ++c)
    for (int i = 0; i < kCombMax; ++i)
      g_comb[slot][c][i] = 0.f;
  for (int i = 0; i < kAp0Len; ++i)
    g_ap0[slot][i] = 0.f;
  for (int i = 0; i < kAp1Len; ++i) {
    g_ap1[slot][i] = 0.f;
    g_ap2[slot][i] = 0.f;
  }
  for (int i = 0; i < kAp4Len; ++i)
    g_ap4[slot][i] = 0.f;
}

// The comb delays carry the size of the object: a violin body answers inside a
// few milliseconds, a room has to be a few metres across before it sounds like
// one. They are coprime-ish on purpose, so the three do not line up into a
// pitch. Three lines and not four: a body is a small object, and the fourth
// line buys density a guitar-sized box does not have.
const float kBodyDelays[kCombs] = {0.0057f, 0.0083f, 0.0119f};

// How loud the resonances and the tail sit in each object, fitted band by band
// to the responses the old version convolved with. The bank is the modes; the
// tail is the diffuse part, and it is what carries a space, which has no modes
// at all.
struct Trim {
  float bank;
  float tail;
};


// Measured on the host: the RMS each body settles at for the same input, in
// dB against the loudest of them. Without these the knob jumps by up to 9 dB
// between bodies (steel is the quiet one), and the crossfade would not hide it.
const float kLevelDb[kPresetCount] = {6.8f, -0.3f, 2.8f, 2.1f,
                                      0.2f, 8.8f, -1.9f, 1.0f};

const Trim kTrim[kPresetCount] = {
    {0.638f, 4.167f},  // violin
    {0.808f, 2.585f},  // cello
    {0.500f, 4.228f},  // piano
    {0.863f, 5.025f},  // guitar
    {1.773f, 3.870f},  // wood
    {0.389f, 5.055f},  // steel
    {1.131f, 5.749f},  // clarinet
    {0.501f, 1.707f},  // drum
};

// Builds one engine for a preset. Only ever called from the parameter callback
// or from init, where a handful of exponentials is fine.
void buildEngine(int slot, float pos) {
  if (pos < 0.f)
    pos = 0.f;
  if (pos > (float)(kPresetCount - 1))
    pos = (float)(kPresetCount - 1);
  const int a = (int)pos;
  const int b = (a + 1 < kPresetCount) ? a + 1 : a;
  const float t = pos - (float)a;
  // The mode set is the nearest body: ratios cannot be blended, and half of one
  // body's partials with half of another's is neither. Everything that is a
  // number — damping, brightness, the loudness of the two halves, the size —
  // glides.
  const int preset = (t < 0.5f) ? a : b;
  const irmodel::Preset &p = irmodel::kPresets[preset];
  // Through the geometric middle of the two, not the arithmetic one: the halves
  // are loudnesses, and linearly the midpoint of two bodies lands a decibel or
  // two above both of them. Two straight segments through their square root
  // cost a sqrt instead of a pow, which this slot has no room for.
  const Trim &trimA = kTrim[a], &trimB = kTrim[b];
  const float gb = arc_sqrtf(trimA.bank * trimB.bank);
  const float gt = arc_sqrtf(trimA.tail * trimB.tail);
  const Trim trim = {(t < 0.5f) ? trimA.bank + (gb - trimA.bank) * (2.f * t)
                                : gb + (trimB.bank - gb) * (2.f * t - 1.f),
                     (t < 0.5f) ? trimA.tail + (gt - trimA.tail) * (2.f * t)
                                : gt + (trimB.tail - gt) * (2.f * t - 1.f)};
  const float size = kBodySize;
  const float level = arc_dbtolevel(kLevelDb[a] + (kLevelDb[b] - kLevelDb[a]) * t);
  // Both bodies' brightness, not the nearest one's: the mode set jumps at the
  // half step, but brightness is a number and glides the whole way.
  const irmodel::Preset &pa = irmodel::kPresets[a];
  const irmodel::Preset &pb = irmodel::kPresets[b];
  const float bright = pa.bright + (pb.bright - pa.bright) * t;
  Engine &e = g_eng[slot];
  clearBuffers(slot);
  clearEngine(e);

  // Resonances. The mode list, its frequencies and its damping come straight
  // from ir_model.h, so a violin here is the violin the old unit convolved with.
  int n = p.mode_count;
  if (n > kMaxModes)
    n = kMaxModes;
  e.modes = n;
  for (int k = 0; k < kMaxModes; ++k) {
    e.a1[k] = 0.f;
    e.a2[k] = 0.f;
    e.mode_gain[k] = 0.f;
  }
  for (int k = 0; k < n; ++k) {
    const irmodel::Mode &m = p.modes[k];
    const float f = p.f0 * m.ratio;
    if (f <= 0.f || f >= 0.45f * kFs)
      continue;

    const float tau_mode = (p.tau > kMaxModeTau) ? kMaxModeTau : p.tau;
    const float r = arc_expneg(1.f / (tau_mode * m.decay * kFs));
    const float w = 2.f * kPi * f / kFs;
    float sw, cw;
    arc_sincos(w, sw, cw);
    e.a1[k] = 2.f * r * cw;
    e.a2[k] = -r * r;
    // The model gives each mode an amplitude in the response. Convolved with a
    // sound, a mode of amplitude A and damping 1 - r emphasises its frequency by
    // A / (2 (1 - r) sin w) after the response is normalised to a peak of 0.5,
    // and a resonator driven at its own frequency settles at exactly that for a
    // gain of A sin w / 2. This is that gain, so the mode lands where the
    // convolution put it.
    e.mode_gain[k] = 0.5f * m.amp * sw * trim.bank;
  }

  // Diffuse tail. The feedback is what the response's decay calls for over the
  // line's own length: a delay line of D samples with feedback g dies with a
  // time constant of -D / ln(g), so g = exp(-D / tau) gives the model's tau.
  const float tauA = (irmodel::kPresets[a].noise_tau > 0.0005f) ? irmodel::kPresets[a].noise_tau : 0.0005f;
  const float tauB = (irmodel::kPresets[b].noise_tau > 0.0005f) ? irmodel::kPresets[b].noise_tau : 0.0005f;
  const float tau = clipminmaxf(0.0005f, tauA + (tauB - tauA) * t, kMaxTailTau);
  for (int c = 0; c < kCombs; ++c) {
    int len = (int)(kBodyDelays[c] * kFs * size);
    if (len > kCombMax)
      len = kCombMax;
    if (len < 16)
      len = 16;
    e.comb_len[c] = len;
    e.comb_fb[c] = clipminmaxf(0.f, arc_expneg((float)len / (tau * kFs)), 0.93f);
  }
  // Brightness is how long the high band lives compared with the low one; in the
  // loop that is a one pole that takes a little more away each pass, and the same
  // one for all four lines.
  {
    const float fc = clipminmaxf(400.f, 500.f + 3500.f * bright, 0.45f * kFs);
    const float damp = 1.f - arc_expneg(2.f * kPi * fc / kFs);
    for (int c = 0; c < kCombs; ++c)
      e.comb_damp[c] = damp;
  }
  const float noiseA = irmodel::kPresets[a].noise, noiseB = irmodel::kPresets[b].noise;
  e.tail_gain = (noiseA + (noiseB - noiseA) * t) * trim.tail;

  // The model's tail is a low band with a high band on top, and the high band is
  // the one that dies first: the split on the way in and the one pole at the end
  // are what let the tail darken as it goes instead of sounding like noise.
  e.tail_cut = 1.f - arc_expneg(2.f * kPi * 450.f / kFs);
  e.tail_hi = 0.2f + 0.6f * bright;
  const float tail_lp = 900.f + 2600.f * bright;
  e.tail_out_cut = 1.f - arc_expneg(2.f * kPi * tail_lp / kFs);

  e.level = level;
}

/*---------------------------------------------------------------------------*
 * Rendering
 *---------------------------------------------------------------------------*/

// Left and right are two orthogonal mixtures of the same four lines, then a
// separate allpass on the difference: the body is one object, so its resonances
// stay in the middle, but the tail arrives from slightly different places.
// Deliberately not inline: the slot has 12 KB of flash for everything, and the
// fade path and the plain path both call this, so inlining it costs a kilobyte.
__attribute__((noinline)) void engineSample(int slot, Engine &e, float x,
                                            float &out_l, float &out_r) {
  // Resonances: two poles, driven by the input, summed with the mode levels.
  float bank = 0.f;
  for (int k = 0; k < e.modes; ++k) {
    const float y = e.a1[k] * e.y1[k] + e.a2[k] * e.y2[k] + x;
    e.y2[k] = e.y1[k];
    e.y1[k] = y;
    bank += e.mode_gain[k] * y;
  }

  // Tail: three damped combs in parallel, then two allpasses to smear them.
  e.tail_lp_in += (x - e.tail_lp_in) * e.tail_cut;
  const float drive = e.tail_lp_in + (x - e.tail_lp_in) * e.tail_hi;

  // Three delay lines in a feedback delay network: each line's output is damped,
  // fed back through a Householder matrix and summed into every other line. That
  // is what makes the tail dense; combs side by side would leave the peaks of
  // their own lengths standing, which sounds like a flange instead of a body.
  float tail = 0.f;
  float side = 0.f;
  float out_sum = 0.f;
  for (int c = 0; c < kCombs; ++c) {
    const float out = g_comb[slot][c][e.comb_pos[c]];
    e.comb_lp[c] += (out - e.comb_lp[c]) * e.comb_damp[c];
    e.comb_fb_out[c] = e.comb_lp[c] * e.comb_fb[c];
    out_sum += e.comb_fb_out[c];
    tail += out;
    int p2 = e.comb_pos[c] + kTap[c];
    if (p2 >= e.comb_len[c]) p2 -= e.comb_len[c];
    side += g_comb[slot][c][p2];
  }
  const float h = out_sum * (2.f / (float)kCombs);
  for (int c = 0; c < kCombs; ++c) {
    float *buf = g_comb[slot][c];
    buf[e.comb_pos[c]] = drive + (e.comb_fb_out[c] - h);
    if (++e.comb_pos[c] >= e.comb_len[c])
      e.comb_pos[c] = 0;
  }
  tail *= 0.25f;

  {
    float *buf = g_ap0[slot];
    const float d = buf[e.ap0_pos];
    const float y = d - kApG * tail;
    buf[e.ap0_pos] = tail + kApG * y;
    if (++e.ap0_pos >= kAp0Len)
      e.ap0_pos = 0;
    tail = y;
  }
  {
    float *buf = g_ap1[slot];
    const float d = buf[e.ap1_pos];
    const float y = d - kApG * tail;
    buf[e.ap1_pos] = tail + kApG * y;
    if (++e.ap1_pos >= kAp1Len)
      e.ap1_pos = 0;
    tail = y;
  }

  {
    float *buf = g_ap2[slot];
    const float d = buf[e.ap2_pos];
    const float y = d - kApG * side;
    buf[e.ap2_pos] = side + kApG * y;
    if (++e.ap2_pos >= kAp1Len)
      e.ap2_pos = 0;
    side = y;
  }

  // The model's tail is a low band with a high band on top of it, and the high
  // band is what dies first: the combs, the split and this one pole are what
  // make the tail darken as it goes instead of sounding like filtered noise.
  e.tail_out_lp += (tail - e.tail_out_lp) * e.tail_out_cut;
  e.tail_out_lp_s += (side - e.tail_out_lp_s) * e.tail_out_cut;

  // Each channel carries the whole body, so both are trimmed to keep the pair
  // from being louder than the mono unit used to be, and the trim is taken once.
  const float bank_trim = kStereoTrim * bank;
  const float mid = bank_trim;
  const float t_m = kStereoTrim * e.tail_out_lp * e.tail_gain;

  float bank_r = bank_trim;
  {
    float *buf = g_ap4[slot];
    const float d = buf[e.ap4_pos];
    const float y = d - kApG4 * bank_r;
    buf[e.ap4_pos] = bank_r + kApG4 * y;
    if (++e.ap4_pos >= kAp4Len)
      e.ap4_pos = 0;
    // Only a little of the rotated copy: two points on a violin are not two
    // different violins.
    bank_r = 0.85f * bank_r + 0.15f * y;
  }

  out_l = (mid + t_m) * e.level;
  out_r = (bank_r + kStereoTrim * e.tail_out_lp_s * e.tail_gain) * e.level;

  // A bank that has gone non-finite — a NaN in a feedback line, a jump that
  // pushed a mode past its own limit — would sit in the slot forever. The gate
  // below would hide it, but it would be back with the next note, so it is
  // dropped here instead.
  if (!(out_l > -kBankLimit && out_l < kBankLimit) ||
      !(out_r > -kBankLimit && out_r < kBankLimit)) {
    clearEngine(e);
    out_l = 0.f;
    out_r = 0.f;
  }
}

void updateDamping() {
  const float fc = 1200.f * arc_pow2f(2.71f * 1.44269504f * s.damp);  // 1.2..18 kHz
  s.damp_coef =
      1.f - arc_expneg(2.f * kPi * clipminmaxf(200.f, fc, 0.45f * kFs) / kFs);
}

// Starts a crossfade into preset p: the free slot is rebuilt, the other one keeps
// playing.
void startFade(float pos) {
  s.pos = pos;
  if (!s.ready) {
    buildEngine(0, pos);
    s.active = 0;
    s.ready = 1;
    return;
  }
  const int slot = (s.active == 0) ? 1 : 0;
  buildEngine(slot, pos);
  s.target = slot;
  s.fade = 0.f;
}

}  // namespace

/*---------------------------------------------------------------------------*
 * Unit hooks
 *---------------------------------------------------------------------------*/

void REVFX_INIT(uint32_t platform, uint32_t api) {
  program_start::reset();
  (void)platform;
  (void)api;

  s.active = 0;
  s.target = -1;
  s.pos = 0.f;
  s.queued = -1.f;
  s.fade = 0.f;
  s.damp = 0.7f;
  s.wet = 0.5f;
  s.damp_lp = 0.f;
  s.damp_lp_r = 0.f;
  s.ready = 0;
  s.gate = 1.f;
  s.quiet = 0;

  updateDamping();
  clearBuffers(0);
  clearBuffers(1);
  buildEngine(0, 0);
  s.ready = 1;
}

void REVFX_PROCESS(float *xn, uint32_t frames) {
  const int nframes = (int)frames;

  if (!s.ready)
    return;

  // The gate shuts 20 ms after the input goes quiet, and from then on the
  // output is zero whatever the bodies are doing. Skips the block, which is what
  // the reverb slot is doing whenever nothing is played.
  if (s.gate <= 0.f && silence::block(xn, (uint32_t)nframes, 2)) {
    for (int i = 0; i < nframes * 2; ++i)
      xn[i] = 0.f;
    return;
  }

  for (int i = 0; i < nframes; ++i) {
    const float in = 0.5f * (xn[2 * i] + xn[2 * i + 1]);

    // A body is not a reverb. Its own tail is 8 to 35 ms, so anything still
    // sounding 20 ms after the input went quiet is state being recited rather
    // than a body: the gate shuts over 10 ms and the unit outputs silence until
    // the next note. A release is never cut by this — the input is only exactly
    // zero once the synth's own envelope has already taken the note down.
    if (in < -kGateFloor || in > kGateFloor) {
      s.quiet = 0;
      s.gate += kGateStep;
      if (s.gate > 1.f)
        s.gate = 1.f;
    } else if (++s.quiet > kGateHold && s.gate > 0.f) {
      s.gate -= kGateStep;
      if (s.gate < 0.f)
        s.gate = 0.f;
    }

    const int sa = s.active;
    float wl, wr;
    if (s.target >= 0) {
      const float f = s.fade;
      float al, ar, bl, br;
      engineSample(sa, g_eng[sa], in, al, ar);
      engineSample(s.target, g_eng[s.target], in, bl, br);
      wl = (1.f - f) * al + f * bl;
      wr = (1.f - f) * ar + f * br;
      s.fade += 1.f / (float)kFadeSamples;
      if (s.fade >= 1.f) {
        s.active = s.target;
        s.target = -1;
        s.fade = 0.f;
        if (s.queued >= 0.f) {
          startFade(s.queued);
          s.queued = -1.f;
        }
      }
    } else {
      engineSample(sa, g_eng[sa], in, wl, wr);
    }

    s.damp_lp += (wl - s.damp_lp) * s.damp_coef;
    s.damp_lp_r += (wr - s.damp_lp_r) * s.damp_coef;
    xn[2 * i] = fx_softclipf(0.05f, s.gate * (in + (s.damp_lp - in) * s.wet));
    xn[2 * i + 1] = fx_softclipf(0.05f, s.gate * (in + (s.damp_lp_r - in) * s.wet));
  }
}

void REVFX_PARAM(uint8_t index, int32_t value) {
  // A program that has never held this effect sends zeroes for its knobs, and
  // zero is time zero or depth zero: the effect would look dead until a knob
  // moved. Until one of them is not zero, it keeps its own start values.
  if (!program_start::chosen(value))
    return;

  switch (index) {
  case k_user_revfx_param_time: {
    // The A knob is a continuum, not eight steps. Between two bodies the mode
    // set is the nearest one but its damping, brightness, level and size glide
    // into the next, so a violin can be walked into a cello. Quarter steps:
    // finer than that is more crossfades than a turn of the knob can use.
    const float pos = clipminmaxf(0.f,
                                  (float)value * (1.f / 1023.f) *
                                      (float)(kPresetCount - 1),
                                  (float)(kPresetCount - 1));
    const float q = (float)((int)(pos * 4.f + 0.5f)) * 0.25f;
    if (q == s.pos)
      break;

    if (s.target >= 0) {
      // While a crossfade runs, remember only the latest request: the knob
      // sends a stream of values and we do not want a crossfade per value.
      s.queued = q;
    } else {
      startFade(q);
    }
    break;
  }

  case k_user_revfx_param_depth:
    s.damp = clipminmaxf(0.f, (float)value * (1.f / 1023.f), 1.f);
    updateDamping();
    break;

  case k_user_revfx_param_shift_depth:
    s.wet = clipminmaxf(0.f, (float)value * (1.f / 1023.f), 1.f);
    break;

  default:
    break;
  }
}

void REVFX_SUSPEND(void) {
  clearBuffers(0);
  clearBuffers(1);
  clearEngine(g_eng[0]);
  clearEngine(g_eng[1]);
  s.target = -1;
  s.queued = -1.f;
  s.fade = 0.f;
  s.damp_lp = 0.f;
  s.damp_lp_r = 0.f;
}

void REVFX_RESUME(void) {}
