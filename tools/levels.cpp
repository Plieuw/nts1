// Host level of one unit, so a patch does not jump when the unit changes.
//
//   levels --knobs N --ranges lo:hi,...  [--note 60]
//
// Built the same way as the harness and the bench (one unit, UNITKIND,
// UNITFILE), and it takes the same --ranges, because the knobs of these units
// do not all run 0..1023.
//
// An oscillator is played one note with no input at all: its level is the RMS
// and the peak of what it makes, measured after the attack has settled. An
// effect is handed the harness's broadband source — the same signal every other
// measurement here uses — and what matters for it is the ratio between what it
// was given and what it hands on: unity.
//
// The reference is the SDK's own: `waves` writes its wavetable at full scale, so
// an oscillator fitted to it peaks where that one does at the same note, and an
// effect fitted to it leaves the level of a dry signal where it found it.
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <cmath>

#include UNITFILE

static const int kBlock = 64;
static const double kRate = 48000.0;
static const int kSignal = 12000;  // 0.25 s: the length of the source
static int gLo[8], gHi[8], gKnobs = 8, gNote = 60;
static bool gPluck = false;

#if UNITKIND == 0
static user_osc_param_t gParam;
static void unitInit() { _hook_init(0, 0x01010000); }
static void unitParam(int index, int value) {
  static const uint16_t ids[6] = {k_user_osc_param_id1, k_user_osc_param_id2,
                                  k_user_osc_param_id3, k_user_osc_param_id4,
                                  k_user_osc_param_id5, k_user_osc_param_id6};
  if (index < 6) _hook_param(ids[index], (uint16_t)value);
  else if (index == 6) _hook_param(k_user_osc_param_shape, (uint16_t)value);
  else _hook_param(k_user_osc_param_shiftshape, (uint16_t)value);
}
static void unitNote(int note) {
  memset(&gParam, 0, sizeof gParam);
  gParam.pitch = (uint16_t)(note << 8);
  _hook_on(&gParam);
}
static void unitRender(float *out, int n) {
  int done = 0;
  while (done < n) {
    int32_t block[kBlock];
    _hook_cycle(&gParam, block, kBlock);
    for (int i = 0; i < kBlock && done < n; ++i)
      out[done++] = (float)block[i] * (1.f / 2147483647.f);
  }
}
#else
static int gPos;
static void unitInit() { _hook_init(0, 0x01010000); gPos = 0; }
static void unitParam(int index, int value) { _hook_param((uint8_t)index, (int32_t)value); }
// Two sources. The default is the harness's broadband one, so a level measured
// here is on the same signal the goldens are on. --pluck plays one note with a
// decay instead: a body, a gate and a resonator answer a single note very
// differently from the way they answer forty eight tones at once, and it is the
// single note a patch is judged on.
static float srcAt(int p) {
  if (p >= kSignal) return 0.f;
  if (gPluck) {
    const double env = exp(-(double)p / 9600.0);  // 0.2 s
    double v = 0.0;
    for (int k = 1; k <= 8; ++k)
      v += 0.20 / k * env * sin(2.0 * M_PI * 220.0 * k * p / kRate);
    return (float)v;
  }
  double v = (p == 0) ? 0.5 : 0.0;
  for (int i = 0; i < 48; ++i) {
    const double f = 80.0 * pow(2.0, i * (6.6 / 47.0));
    v += 0.006 * sin(2.0 * M_PI * f * p / kRate + i * 1.7);
  }
  return (float)v;
}
static void unitRender(float *out, int n) {
  int done = 0;
  while (done < n) {
#if UNITKIND == 1
    float in[kBlock], yl[kBlock], yr[kBlock];
    for (int i = 0; i < kBlock; ++i) in[i] = srcAt(gPos + i);
    _hook_process(in, yl, in, yr, kBlock);
    for (int i = 0; i < kBlock && done < n; ++i)
      out[done++] = 0.5f * (yl[i] + yr[i]);
#else
    float io[2 * kBlock];
    for (int i = 0; i < kBlock; ++i) io[2 * i] = io[2 * i + 1] = srcAt(gPos + i);
    _hook_process(io, kBlock);
    for (int i = 0; i < kBlock && done < n; ++i)
      out[done++] = 0.5f * (io[2 * i] + io[2 * i + 1]);
#endif
    gPos += kBlock;
  }
}
#endif

static double rmsOf(const float *x, int n) {
  double e = 0;
  for (int i = 0; i < n; ++i) e += (double)x[i] * x[i];
  return sqrt(e / n);
}

static double peakOf(const float *x, int n) {
  double p = 0;
  for (int i = 0; i < n; ++i) {
    const double a = fabs(x[i]);
    if (a > p) p = a;
  }
  return p;
}

int main(int argc, char **argv) {
  for (int i = 0; i < 8; ++i) { gLo[i] = 0; gHi[i] = 1023; }
  for (int a = 1; a < argc; ++a) {
    if (strcmp(argv[a], "--knobs") == 0) { gKnobs = atoi(argv[++a]); continue; }
    if (strcmp(argv[a], "--note") == 0) { gNote = atoi(argv[++a]); continue; }
    if (strcmp(argv[a], "--pluck") == 0) { gPluck = true; continue; }
    if (strcmp(argv[a], "--ranges") != 0) continue;
    const char *p = argv[a + 1];
    for (int i = 0; i < 8 && *p; ++i) {
      gLo[i] = (int)strtol(p, (char **)&p, 10);
      if (*p == ':') ++p;
      gHi[i] = (int)strtol(p, (char **)&p, 10);
      if (*p == ',') ++p;
    }
  }

  static float out[48000];
  unitInit();
  for (int i = 0; i < gKnobs; ++i) unitParam(i, (gLo[i] + gHi[i]) / 2);

#if UNITKIND == 0
  unitNote(gNote);
  unitRender(out, 48000);
  // From 50 ms in: the attack of a bowed or struck model is not what a patch is
  // compared on, and its peak is.
  const int from = 2400;
  printf("note %d rms %.4f peak %.4f\n", gNote,
         rmsOf(out + from, 48000 - from), peakOf(out, 48000));
#else
  unitRender(out, 48000);  // the source, then 0.75 s of silence after it
  static float in[kSignal];
  for (int i = 0; i < kSignal; ++i) in[i] = srcAt(i);
  const double ri = rmsOf(in, kSignal), ro = rmsOf(out, kSignal);
  printf("in_rms %.4f out_rms %.4f out_peak %.4f gain_db %+.2f\n", ri, ro,
         peakOf(out, kSignal), 20.0 * log10(ro / ri));
#endif
  return 0;
}
