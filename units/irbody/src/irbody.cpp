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
 *   diffuse tail    four damped comb filters, feedback from the tail's decay
 *   size            the comb delays: short for a body, longer for a space
 *   damping         the same one pole low pass on the wet signal as before
 *
 * The sustain is the one difference worth knowing about. An 85 ms response stops
 * 85 ms after the sound that fed it; a network keeps ringing for as long as it
 * is fed, so a held note sits inside the body instead of leaving it after a few
 * frames. That is what a body does, and it is the one place where this is not
 * only cheaper but better.
 *
 * A-knob (TIME)   which object: violin, cello, piano, guitar, wood, steel,
 *                 room, plate
 * B-knob (DEPTH)  damping of the wet signal, dark .. open
 * DEL + B knob    dry/wet mix
 *
 * Two engines run during a switch and their outputs are crossfaded over 85 ms,
 * so turning the knob cannot click. Everything is set up in the parameter
 * callback, never in the audio callback.
 */

#include "userrevfx.h"

#include "ir_model.h"

namespace {

constexpr float kFs = 48000.f;
constexpr float kPi = 3.14159265358979f;

constexpr int kMaxModes = 11;   // the widest mode set in ir_model.h
constexpr int kCombs = 4;
constexpr int kCombMax = 1400;  // 29 ms at 48 kHz, the longest comb
constexpr int kAp0Len = 240;    // 5.0 ms allpass
constexpr int kAp1Len = 96;     // 2.0 ms allpass
constexpr int kErMax = 800;     // 16.7 ms of early reflections
constexpr int kErTaps = 10;
constexpr int kPresetCount = irmodel::kPresetCount;
constexpr int kFadeSamples = 4096;  // 85 ms crossfade on a switch

// The old unit normalised every response to a peak of 0.5 and convolved with it.
// A mode of amplitude A and damping 1 - r has a resonant gain of A / (2 (1 - r))
// in that response, which is what these two constants put back: the bank is
// scaled to the same resonant gain, so a violin still emphasises the notes a
// violin emphasises by the same amount.
constexpr float kBankScale = 1.f;
constexpr float kTailScale = 1.f;

// The .sdram section is not zeroed at load time, so every buffer is cleared
// explicitly before it is used.
float g_comb[2][kCombs][kCombMax] __sdram;
float g_ap0[2][kAp0Len] __sdram;
float g_ap1[2][kAp1Len] __sdram;
float g_er[2][kErMax] __sdram;

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
  float bank_scale;

  // Diffusion of the tail.
  int ap0_pos;
  int ap1_pos;
  float ap_g;

  // The model's tail is a low band plus a high band that dies away faster; the
  // split is a one pole at 450 Hz on the way in.
  float tail_cut;
  float tail_hi;
  float tail_lp_in;
  float tail_out_cut;
  float tail_out_lp;
  float er_out;

  // Early reflections: the discrete bounces that give a space its size, and the
  // only part of a room the model had that a diffuse network cannot invent.
  int er_taps;
  int er_len;
  int er_pos;
  float er_gain[kErTaps];
  int er_tap[kErTaps];
};

Engine g_eng[2];

struct State {
  int active;   // engine slot in the output
  int target;   // engine slot fading in, -1 when not fading
  int queued;   // last request that arrived during a fade, -1 when none
  float fade;   // 0..1, position of the crossfade
  float damp;   // 0..1
  float wet;    // 0..1
  float damp_coef;
  float damp_lp;
  int ready;    // set once the first engine has been built
};

State s;

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
  e.er_out = 0.f;
  e.ap0_pos = 0;
  e.ap1_pos = 0;
  e.er_taps = 0;
  e.er_len = kErMax;
  e.er_pos = 0;
}

void clearBuffers(int slot) {
  for (int c = 0; c < kCombs; ++c)
    for (int i = 0; i < kCombMax; ++i)
      g_comb[slot][c][i] = 0.f;
  for (int i = 0; i < kAp0Len; ++i)
    g_ap0[slot][i] = 0.f;
  for (int i = 0; i < kAp1Len; ++i)
    g_ap1[slot][i] = 0.f;
  for (int i = 0; i < kErMax; ++i)
    g_er[slot][i] = 0.f;
}

// The comb delays carry the size of the object: a violin body answers inside a
// few milliseconds, a room has to be a few metres across before it sounds like
// one. They are coprime-ish on purpose, so the four do not line up into a pitch.
const float kBodyDelays[kCombs] = {0.0057f, 0.0083f, 0.0119f, 0.0157f};
const float kSpaceDelays[kCombs] = {0.0091f, 0.0133f, 0.0179f, 0.0233f};

// How loud the resonances and the tail sit in each object, fitted band by band
// to the responses the old version convolved with. The bank is the modes; the
// tail is the diffuse part, and it is what carries a space, which has no modes
// at all.
struct Trim {
  float bank;
  float tail;
};

const Trim kTrim[kPresetCount] = {
    {0.638f, 4.167f},  // violin
    {0.808f, 2.585f},  // cello
    {0.500f, 4.228f},  // piano
    {0.863f, 5.025f},  // guitar
    {1.773f, 3.870f},  // wood
    {0.389f, 5.055f},  // steel
    {0.000f, 10.439f},  // room
    {0.000f, 8.897f},  // plate
};

// Builds one engine for a preset. Only ever called from the parameter callback
// or from init, where a handful of exponentials is fine.
void buildEngine(int slot, int preset) {
  if (preset < 0)
    preset = 0;
  if (preset >= kPresetCount)
    preset = kPresetCount - 1;

  const irmodel::Preset &p = irmodel::kPresets[preset];
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

    const float r = expf(-1.f / (p.tau * m.decay * kFs));
    const float w = 2.f * kPi * f / kFs;
    e.a1[k] = 2.f * r * cosf(w);
    e.a2[k] = -r * r;
    // The model gives each mode an amplitude in the response. Convolved with a
    // sound, a mode of amplitude A and damping 1 - r emphasises its frequency by
    // A / (2 (1 - r) sin w) after the response is normalised to a peak of 0.5,
    // and a resonator driven at its own frequency settles at exactly that for a
    // gain of A sin w / 2. This is that gain, so the mode lands where the
    // convolution put it.
    e.mode_gain[k] = 0.5f * m.amp * sinf(w) * kBankScale * kTrim[preset].bank;
  }

  // Diffuse tail. The feedback is what the response's decay calls for over the
  // line's own length: a delay line of D samples with feedback g dies with a
  // time constant of -D / ln(g), so g = exp(-D / tau) gives the model's tau.
  const bool space = (p.mode_count == 0);
  const float *delays = space ? kSpaceDelays : kBodyDelays;
  const float tau = (p.noise_tau > 0.0005f) ? p.noise_tau : 0.0005f;
  for (int c = 0; c < kCombs; ++c) {
    int len = (int)(delays[c] * kFs);
    if (len > kCombMax)
      len = kCombMax;
    if (len < 16)
      len = 16;
    e.comb_len[c] = len;
    e.comb_fb[c] = clipminmaxf(0.f, expf(-(float)len / (tau * kFs)), 0.93f);

    // Brightness is how long the high band lives compared with the low one; in
    // the loop that is a one pole that takes a little more away each pass.
    const float fc = clipminmaxf(400.f, 500.f + 3500.f * p.bright, 0.45f * kFs);
    e.comb_damp[c] = 1.f - expf(-2.f * kPi * fc / kFs);
  }
  e.tail_gain = p.noise * kTailScale * kTrim[preset].tail;

  // The model's tail is a low band with a high band on top, and the high band is
  // the one that dies first: the split on the way in and the one pole at the end
  // are what let the tail darken as it goes instead of sounding like noise.
  e.tail_cut = 1.f - expf(-2.f * kPi * 450.f / kFs);
  e.tail_hi = 0.2f + 0.6f * p.bright;
  // A space is heard from the room, a body from the inside: the same brightness
  // leaves a space with more air on top than the diffuse field of a body.
  const float tail_lp = space ? (2000.f + 6000.f * p.bright)
                              : (900.f + 2600.f * p.bright);
  e.tail_out_cut = 1.f - expf(-2.f * kPi * tail_lp / kFs);

  // Early reflections for the spaces. A body answers from its first sample and
  // does not need them; a room is quiet until the first wall, which is exactly
  // the part a diffuse tail alone cannot give.
  if (space) {
    uint32_t rng = 0x9E3779B9u + 7919u * (uint32_t)preset;
    int taps = 0;
    for (int r = 0; r < kErTaps; ++r) {
      rng = rng * 1664525u + 1013904223u;
      const float u = (float)(rng >> 8) * (1.f / 16777216.f);
      const int d = (int)((0.002f + 0.013f * u) * kFs);
      if (d >= kErMax)
        continue;
      rng = rng * 1664525u + 1013904223u;
      const float g = (float)(rng >> 8) * (2.f / 16777216.f) - 1.f;
      e.er_tap[taps] = d;
      e.er_gain[taps] = g * 0.4f * expf(-0.25f * (float)r);
      ++taps;
    }
    e.er_taps = taps;
  }

  e.ap_g = 0.6f;
  e.bank_scale = kBankScale;
}

/*---------------------------------------------------------------------------*
 * Rendering
 *---------------------------------------------------------------------------*/

inline float engineSample(int slot, Engine &e, float x) {
  // Resonances: two poles, driven by the input, summed with the mode levels.
  float bank = 0.f;
  for (int k = 0; k < e.modes; ++k) {
    const float y = e.a1[k] * e.y1[k] + e.a2[k] * e.y2[k] + x;
    e.y2[k] = e.y1[k];
    e.y1[k] = y;
    bank += e.mode_gain[k] * y;
  }
  bank *= e.bank_scale;

  // Tail: four damped combs in parallel, then two allpasses to smear them.
  // Early reflections come off a short delay line tapping the same input.
  if (e.er_taps > 0) {
    float *const er = g_er[slot];
    er[e.er_pos] = x;
    float er_sum = 0.f;
    for (int t = 0; t < e.er_taps; ++t) {
      int idx = e.er_pos - e.er_tap[t];
      if (idx < 0)
        idx += kErMax;
      er_sum += er[idx] * e.er_gain[t];
    }
    if (++e.er_pos >= kErMax)
      e.er_pos = 0;
    e.er_out = er_sum;
  }

  e.tail_lp_in += (x - e.tail_lp_in) * e.tail_cut;
  const float drive = e.tail_lp_in + (x - e.tail_lp_in) * e.tail_hi;

  // Four delay lines in a feedback delay network: each line's output is damped,
  // fed back through a Householder matrix and summed into every other line. That
  // is what makes the tail dense; four combs side by side would leave the peaks
  // of their own lengths standing, which sounds like a flange instead of a room.
  float tail = 0.f;
  float out_sum = 0.f;
  for (int c = 0; c < kCombs; ++c) {
    const float out = g_comb[slot][c][e.comb_pos[c]];
    e.comb_lp[c] += (out - e.comb_lp[c]) * e.comb_damp[c];
    e.comb_fb_out[c] = e.comb_lp[c] * e.comb_fb[c];
    out_sum += e.comb_fb_out[c];
    tail += out;
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
    const float y = d - e.ap_g * tail;
    buf[e.ap0_pos] = tail + e.ap_g * y;
    if (++e.ap0_pos >= kAp0Len)
      e.ap0_pos = 0;
    tail = y;
  }
  {
    float *buf = g_ap1[slot];
    const float d = buf[e.ap1_pos];
    const float y = d - e.ap_g * tail;
    buf[e.ap1_pos] = tail + e.ap_g * y;
    if (++e.ap1_pos >= kAp1Len)
      e.ap1_pos = 0;
    tail = y;
  }

  // The model's tail is a low band with a high band on top of it, and the high
  // band is what dies first: the combs, the split and this one pole are what
  // make the tail darken as it goes instead of sounding like filtered noise.
  e.tail_out_lp += (tail - e.tail_out_lp) * e.tail_out_cut;

  return bank + (e.tail_out_lp + e.er_out) * e.tail_gain;
}

void updateDamping() {
  const float fc = 1200.f * expf(2.71f * s.damp);  // 1.2 kHz .. 18 kHz
  s.damp_coef =
      1.f - expf(-2.f * kPi * clipminmaxf(200.f, fc, 0.45f * kFs) / kFs);
}

// Starts a crossfade into preset p: the free slot is rebuilt, the other one keeps
// playing.
void startFade(int preset) {
  if (!s.ready) {
    buildEngine(0, preset);
    s.active = 0;
    s.ready = 1;
    return;
  }
  const int slot = (s.active == 0) ? 1 : 0;
  buildEngine(slot, preset);
  s.target = slot;
  s.fade = 0.f;
}

}  // namespace

/*---------------------------------------------------------------------------*
 * Unit hooks
 *---------------------------------------------------------------------------*/

void REVFX_INIT(uint32_t platform, uint32_t api) {
  (void)platform;
  (void)api;

  s.active = 0;
  s.target = -1;
  s.queued = -1;
  s.fade = 0.f;
  s.damp = 0.7f;
  s.wet = 0.5f;
  s.damp_lp = 0.f;
  s.ready = 0;

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

  for (int i = 0; i < nframes; ++i) {
    const float in = 0.5f * (xn[2 * i] + xn[2 * i + 1]);

    const int sa = s.active;
    float wet;
    if (s.target >= 0) {
      const float f = s.fade;
      wet = (1.f - f) * engineSample(sa, g_eng[sa], in) +
            f * engineSample(s.target, g_eng[s.target], in);
      s.fade += 1.f / (float)kFadeSamples;
      if (s.fade >= 1.f) {
        s.active = s.target;
        s.target = -1;
        s.fade = 0.f;
        if (s.queued >= 0) {
          const int p = s.queued;
          s.queued = -1;
          startFade(p);
        }
      }
    } else {
      wet = engineSample(sa, g_eng[sa], in);
    }

    s.damp_lp += (wet - s.damp_lp) * s.damp_coef;
    const float y = in + (s.damp_lp - in) * s.wet;
    xn[2 * i] = fx_softclipf(0.05f, y);
    xn[2 * i + 1] = xn[2 * i];
  }
}

void REVFX_PARAM(uint8_t index, int32_t value) {
  switch (index) {
  case k_user_revfx_param_time: {
    const int sel = (int)clipminmaxf(
        0.f, (float)value * (1.f / 1023.f) * (float)kPresetCount,
        (float)(kPresetCount - 1));

    if (sel == s.active || sel == s.target)
      break;

    if (s.target >= 0) {
      // While a crossfade runs, remember only the latest request: the knob
      // sends a stream of values and we do not want a crossfade per value.
      s.queued = sel;
    } else {
      startFade(sel);
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
  s.queued = -1;
  s.fade = 0.f;
  s.damp_lp = 0.f;
}

void REVFX_RESUME(void) {}
