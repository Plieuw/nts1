/*
 * reflect.cpp -- a space made of delays and reflections, for the delay slot.
 *
 * BSD 3-Clause License, see LICENSE.
 *
 * Every other space here either convolved with a rendered response or built its
 * resonances from a model. This one has neither: there is no impulse response
 * stored anywhere in it, and no resonators. What it has is four short delay
 * lines between them, three to nine milliseconds long, feeding each other back
 * through a matrix, and two allpasses that smear the input before it reaches
 * them. Its impulse response is not a file that was loaded; it is the train of
 * reflections those lines actually produce, and it changes with the knobs the way
 * a room of a different size does.
 *
 * That is what a small space does, physically. A sound hits four walls at four
 * distances, each one comes back a little later and a little quieter, and every
 * later bounce is a mixture of all the ones before it. The unit plays that
 * directly:
 *
 *   the walls    four taps off the input history, half a millisecond to two,
 *                alternating in sign, so the first thing heard is discrete
 *                reflections rather than a comb
 *   the space    four delay lines, 3.8 ms to 9.4 ms at full size, each damped,
 *                all fed back through a Householder matrix
 *   the haze     two allpasses in front of the network, so what goes into the
 *                lines is already smeared instead of being one clean click
 *
 * A-knob (TIME)   size: the lines and the reflections scale from 0.35x to 1x
 * B-knob (DEPTH)  decay: how many times the reflections keep coming back
 * DEL + B knob    dry/wet mix
 *
 * Why the delay slot: this is a space, not a body, so it answers over seconds
 * rather than milliseconds, and a delay slot has the 12 KB and the memory a tail
 * that long needs. It also puts it where a stock reverb can follow it.
 *
 * The size knob moves the read distance of each line continuously rather than
 * jumping between lengths. Turning a delay length in steps is a click; moving it
 * as a slope is the pitch glide a tape delay makes, which is the sound a delay
 * line is supposed to make when you move it. Everything else the knobs do is a
 * gain and is smoothed the same way, 30 ms, so nothing steps.
 */

#include "userdelfx.h"
#include "../../common/program_start.h"
#include "../../common/silence.h"
#include "../../common/arcmath.h"

namespace {

constexpr float kFs = 48000.f;
constexpr float kPi = 3.14159265358979f;

// --- the lines -------------------------------------------------------------

constexpr int kLines = 4;

// Lengths at full size, in samples: 181, 263, 373 and 449 are 3.8, 5.5, 7.8 and
// 9.4 ms, which is a room about three metres across at the plate. They are not
// in a ratio to each other on purpose: lines that are simple multiples of each
// other line their reflections up into a pitch, and a space is not tuned.
constexpr float kBaseDelay[kLines] = {181.f, 263.f, 373.f, 449.f};
// Each ring is the longest delay it can be asked for plus two samples, rounded
// up: the interpolating read touches one sample before the distance, so nothing
// may wrap into the write position.
constexpr int kCap[kLines] = {192, 272, 384, 456};
float g_ring0[kCap[0]];
float g_ring1[kCap[1]];
float g_ring2[kCap[2]];
float g_ring3[kCap[3]];
float *const g_line[kLines] = {g_ring0, g_ring1, g_ring2, g_ring3};

// The two channels are two points of the same room: left weighs the four walls
// equally, right gives the even ones a plus and the odd ones a minus. That is a
// rotation, so it costs no level, and it is why the pair is wide without the
// two sides being out of phase at any frequency -- a delay between the channels
// would cancel in mono, a different weighting of the same reflections does not.
constexpr float kMix = 0.6f;   // of the plain sum
constexpr float kSpread = 0.78f;  // of the signed sum, one way or the other

// One pole in every feedback path, at about 5 kHz: the reflections that come
// back are the ones that bounced off something soft. It is what keeps a small
// room from sounding like a metal box, which a lossless feedback network does.
constexpr float kDampHz = 5000.f;

// --- the walls -------------------------------------------------------------

constexpr int kInLen = 128;  // power of two: the wrap is a mask
float g_in[kInLen];
constexpr int kTaps = 4;
// 0.48, 0.85, 1.4 and 2.0 ms: four reflections off four surfaces at four
// distances, with alternating signs so their sum is a space and not one echo.
constexpr int kTapDelay[kTaps] = {23, 41, 67, 97};
constexpr float kTapGain[kTaps] = {0.80f, -0.52f, 0.36f, -0.22f};
constexpr float kTapTrim = 0.25f;

// --- the diffusers ---------------------------------------------------------

constexpr int kApPasses = 2;
constexpr int kApLen[kApPasses] = {61, 113};
float g_ap1[kApLen[0]];
float g_ap2[kApLen[1]];
float *const g_ap[kApPasses] = {g_ap1, g_ap2};
constexpr float kApG = 0.62f;

// --- levels and limits -----------------------------------------------------

// The wet path is normalised to the middle of the decay knob rather than to the
// top: a network with more feedback stores more energy, so without this the B
// knob would be a volume control. (1 - fb) is what roughly holds the settled
// level still, and the constant is the trim measured at the middle with
// tools/levels.cpp --pluck.
constexpr float kNetTrim = 0.42f;
constexpr float kFbRef = 0.874f;  // what the middle of the decay knob asks for
// A space never sums this loud; anything past it is a runaway, not a sound.
constexpr float kLimit = 9.f;
// Idle: the input is exactly zero and the network holds less than this on
// average. Then the block is skipped and written as zeroes, which is what the
// delay slot is being handed between notes.
constexpr float kIdleMag = 1e-6f;
// Below this the rings are dropped rather than left to shrink into denormals,
// which the FPU would process at a fraction of its speed.
constexpr float kFlushMag = 1e-9f;
// One pole, 30 ms: the glide both the line lengths and the gains take.
constexpr float kGlide = 1.f - 0.00069f;

struct State {
  int pos[kLines];      // write position in each ring
  float delay[kLines];  // current read distance, in samples (a float: it glides)
  float delay_target[kLines];
  float lp[kLines];     // one pole in each feedback path
  int in_pos;
  int ap_pos[kApPasses];
  float fb;
  float fb_target;
  float wet;
  float wet_target;
  float mag;  // what the wet path put out last block, for the idle test
};

State s;

void clearBuffers() {
  for (int k = 0; k < kLines; ++k) {
    float *buf = g_line[k];
    for (int i = 0; i < kCap[k]; ++i)
      buf[i] = 0.f;
    s.pos[k] = 0;
    s.lp[k] = 0.f;
  }
  for (int a = 0; a < kApPasses; ++a) {
    float *buf = g_ap[a];
    for (int i = 0; i < kApLen[a]; ++i)
      buf[i] = 0.f;
    s.ap_pos[a] = 0;
  }
  for (int i = 0; i < kInLen; ++i)
    g_in[i] = 0.f;
  s.in_pos = 0;
  s.mag = 0.f;
}

// The size knob, as the distance each line reads at: 0.35x to 1x of the lengths
// above, which is a room from one metre to three.
void setSize(float v) {
  const float scale = 0.35f + 0.65f * v;
  for (int k = 0; k < kLines; ++k)
    s.delay_target[k] = kBaseDelay[k] * scale;
}

inline float dampCoef() {
  // 1 - exp(-a) would be wrong at this a (2 pi 5000 / 48000 is 0.65, where the
  // series is still fine, but the exponential form is what the other units use
  // and it costs a call to the same polynomial).
  return 1.f - arc_expneg(2.f * kPi * kDampHz / kFs);
}

}  // namespace

/*---------------------------------------------------------------------------*
 * Unit hooks
 *---------------------------------------------------------------------------*/

void DELFX_INIT(uint32_t platform, uint32_t api) {
  program_start::reset();
  (void)platform;
  (void)api;

  s.fb = 0.874f;
  s.fb_target = 0.874f;
  s.wet = 0.45f;
  s.wet_target = 0.45f;
  setSize(0.5f);
  for (int k = 0; k < kLines; ++k)
    s.delay[k] = s.delay_target[k];

  clearBuffers();
}

void DELFX_PROCESS(float *xn, uint32_t frames) {
  const int n = (int)frames;

  // Digital silence in and a network that has run down: every term is zero, so
  // the block is zero. That is the state the delay slot sits in between notes.
  if (silence::block(xn, frames, 2) && s.mag < kIdleMag) {
    for (int i = 0; i < n * 2; ++i)
      xn[i] = 0.f;
    if (s.mag < kFlushMag)
      clearBuffers();
    return;
  }

  const float damp = dampCoef();
  const float net_scale = kNetTrim / (1.f - kFbRef);

  float fb = s.fb;
  float wet = s.wet;
  float dly[kLines];
  for (int k = 0; k < kLines; ++k)
    dly[k] = s.delay[k];

  float mag = 0.f;

  for (int i = 0; i < n; ++i) {
    const float in = 0.5f * (xn[2 * i] + xn[2 * i + 1]);

    // The walls: the input read back at four short distances. The history is
    // written every sample even when the wet knob is down, because a knob that
    // comes up must find the last two milliseconds rather than nothing.
    const int ip = s.in_pos;
    g_in[ip] = in;
    float early_l = 0.f;
    float early_r = 0.f;
    for (int t = 0; t < kTaps; ++t) {
      const float v = g_in[(ip - kTapDelay[t]) & (kInLen - 1)];
      const float w = g_in[(ip - kTapDelay[t] - 1) & (kInLen - 1)];
      early_l += kTapGain[t] * v;
      early_r += kTapGain[t] * w;
    }
    s.in_pos = (ip + 1) & (kInLen - 1);

    // The haze: two allpasses, so what enters the room is a smear and not a
    // click. An allpass keeps the level, so this costs no gain staging.
    float x = in;
    for (int a = 0; a < kApPasses; ++a) {
      float *buf = g_ap[a];
      const int p = s.ap_pos[a];
      const float d = buf[p];
      const float v = x + kApG * d;
      buf[p] = v;
      x = d - kApG * v;
      s.ap_pos[a] = (p + 1 == kApLen[a]) ? 0 : p + 1;
    }

    // The room: read the four lines at their distances, damp them, and feed
    // them back through a Householder matrix. Every line then receives a little
    // of all four, which is what makes the reflections pile up into a tail
    // instead of standing as four combs.
    float y[kLines];
    for (int k = 0; k < kLines; ++k) {
      const float d = dly[k];
      const int di = (int)d;
      const float fr = d - (float)di;
      int rp = s.pos[k] - di;
      if (rp < 0)
        rp += kCap[k];
      int rp1 = rp - 1;
      if (rp1 < 0)
        rp1 += kCap[k];
      const float *buf = g_line[k];
      y[k] = buf[rp] + fr * (buf[rp1] - buf[rp]);
    }

    float sum = 0.f;
    for (int k = 0; k < kLines; ++k) {
      s.lp[k] += (y[k] - s.lp[k]) * damp;
      sum += s.lp[k];
    }
    const float plain = y[0] + y[1] + y[2] + y[3];
    const float alt = -y[0] + y[1] - y[2] + y[3];
    const float out_l = kMix * plain + kSpread * alt;
    const float out_r = kMix * plain - kSpread * alt;
    const float h = (2.f / (float)kLines) * sum;
    for (int k = 0; k < kLines; ++k) {
      float *buf = g_line[k];
      buf[s.pos[k]] = x + fb * (s.lp[k] - h);
      const int p = s.pos[k] + 1;
      s.pos[k] = (p == kCap[k]) ? 0 : p;
    }

    // The level of the wet path follows the decay: with more feedback the
    // network stores more, and without this the B knob would just make it
    // louder.
    const float net = net_scale * (1.f - fb);
    float wl = wet * (kTapTrim * early_l + net * out_l);
    float wr = wet * (kTapTrim * early_r + net * out_r);

    // A network that has gone non-finite -- a NaN in a ring, a jump that pushed
    // the feedback past its own limit -- would sit in the slot forever. Drop
    // it; the input re-excites it on the next sample.
    float ol = in + wl;
    float orr = in + wr;
    if (!(ol > -kLimit && ol < kLimit) || !(orr > -kLimit && orr < kLimit)) {
      clearBuffers();
      ol = in;
      orr = in;
      wl = wr = 0.f;
    }

    xn[2 * i] = fx_softclipf(0.05f, ol);
    xn[2 * i + 1] = fx_softclipf(0.05f, orr);
    mag += fabsf(wl) + fabsf(wr);

    // The knobs, per sample: the lines glide to their new distances, the two
    // gains to theirs. A knob turn is then a slope and not a step.
    for (int k = 0; k < kLines; ++k) {
      const float t = s.delay_target[k];
      if (dly[k] != t) {
        const float d2 = t + (dly[k] - t) * kGlide;
        dly[k] = (fabsf(d2 - t) < 0.02f) ? t : d2;
      }
    }
    fb += (s.fb_target - fb) * kGlide;
    wet += (s.wet_target - wet) * kGlide;
  }

  for (int k = 0; k < kLines; ++k)
    s.delay[k] = dly[k];
  s.fb = fb;
  s.wet = wet;
  s.mag = mag * (1.f / (float)(2 * n));
}

void DELFX_PARAM(uint8_t index, int32_t value) {
  // A program that has never held this effect sends zeroes for its knobs, and
  // zero is a room of no size and a mix of nothing: the unit would look dead
  // until a knob moved. Until one of them is not zero it keeps its own values.
  if (!program_start::chosen(value))
    return;

  const float v = clipminmaxf(0.f, (float)value * (1.f / 1023.f), 1.f);

  switch (index) {
  case k_user_delfx_param_time:
    setSize(v);
    break;

  case k_user_delfx_param_depth:
    // 0.62 to 0.98, and square-rooted so the middle of the knob is a room of
    // about 0.4 s rather than of 0.15: at the bottom the reflections come back
    // twice and stop, at the top the room keeps answering for a couple of
    // seconds.
    s.fb_target = 0.62f + 0.36f * arc_sqrtf(v);
    break;

  case k_user_delfx_param_shift_depth:
    s.wet_target = v;
    break;

  default:
    // k_user_delfx_param_reserved0: nothing on the mkI's delay page sends it.
    break;
  }
}

void DELFX_SUSPEND(void) {
  clearBuffers();
  s.fb = s.fb_target;
  s.wet = s.wet_target;
  for (int k = 0; k < kLines; ++k)
    s.delay[k] = s.delay_target[k];
}

void DELFX_RESUME(void) {}
