// Do the oscillators slur? A second key while the first is still down has to
// move the pitch to the new note without restarting the sound.
//
//   legato --unit bowed          prints key/value lines, checked by run.sh
//
// Built like the harness (UNITKIND=0, UNITFILE), set up per unit by name, and
// measured on the output only: the pitch by autocorrelation, the level by RMS,
// and for a noise source the two notes by their energy at each frequency.
#include <cstdio>
#include <cstring>
#include <cmath>
#include UNITFILE

static const int kBlock = 64;
static const int kRate = 48000;
static float gBuf[24000];
static user_osc_param_t gParam;

static void render(int frames, int pitch) {
  for (int done = 0; done < frames;) {
    const int n = (frames - done < kBlock) ? frames - done : kBlock;
    memset(&gParam, 0, sizeof gParam);
    gParam.pitch = (uint16_t)pitch;
    int32_t block[kBlock];
    memset(block, 0, sizeof block);
    _hook_cycle(&gParam, block, kBlock);
    for (int i = 0; i < n; ++i)
      gBuf[done + i] = (float)block[i] * (1.f / 2147483648.f);
    done += n;
  }
}

static void noteOn(int pitch) {
  user_osc_param_t p;
  memset(&p, 0, sizeof p);
  p.pitch = (uint16_t)pitch;
  _hook_on(&p);
}

static void noteOff() {
  user_osc_param_t p;
  memset(&p, 0, sizeof p);
  _hook_off(&p);
}

static float rms(int start, int len) {
  double a = 0.0;
  for (int i = 0; i < len; ++i) a += (double)gBuf[start + i] * gBuf[start + i];
  return (float)sqrt(a / len);
}

// One period, from the first maximum of the autocorrelation after it has crossed
// zero: every multiple of the period is a peak too and the tallest is not always
// the first.
static float periodOf(int start, int len) {
  static double acc[400];
  for (int lag = 0; lag < 400; ++lag) {
    double a = 0.0;
    for (int i = 0; i < len - lag; ++i) a += (double)gBuf[start + i] * gBuf[start + i + lag];
    acc[lag] = a;
  }
  int lag = 1;
  while (lag < 399 && acc[lag] > 0.0) ++lag;
  double best = acc[lag];
  int bestLag = lag;
  for (; lag < 400; ++lag)
    if (acc[lag] > best) { best = acc[lag]; bestLag = lag; }
  return (float)bestLag;
}

static double energyAt(int start, int len, double f) {
  double re = 0.0, im = 0.0;
  for (int i = 0; i < len; ++i) {
    const double w = 6.283185307179586 * f * (double)i / (double)kRate;
    re += gBuf[start + i] * cos(w);
    im += gBuf[start + i] * sin(w);
  }
  return sqrt(re * re + im * im) / len;
}

int main(int argc, char **argv) {
  const char *unit = "";
  for (int a = 1; a < argc; ++a)
    if (strcmp(argv[a], "--unit") == 0) unit = argv[++a];

  _hook_init(0, 0x01010000);
  if (strcmp(unit, "noise") == 0) {
    _hook_param(k_user_osc_param_id1, 6);     // tuned: the pitched algorithm
    _hook_param(k_user_osc_param_id2, 0);
    _hook_param(k_user_osc_param_id3, 100);   // key tracking on
    _hook_param(k_user_osc_param_id4, 50);
    _hook_param(k_user_osc_param_id5, 100);
    _hook_param(k_user_osc_param_id6, 50);
  } else if (strcmp(unit, "resonator") == 0) {
    _hook_param(k_user_osc_param_id1, 0);     // string
    _hook_param(k_user_osc_param_id2, 40);
    _hook_param(k_user_osc_param_id3, 30);
    _hook_param(k_user_osc_param_id5, 100);
    _hook_param(k_user_osc_param_id6, 20);
  } else {
    _hook_param(k_user_osc_param_id1, 0);     // bowed: string
    _hook_param(k_user_osc_param_id2, 50);
    _hook_param(k_user_osc_param_id3, 60);
    _hook_param(k_user_osc_param_id5, 100);
  }

  noteOn(60 * 256);
  render(6000, 60 * 256);                    // 125 ms of C4
  const float before = periodOf(3000, 3000);
  const float levelBefore = rms(5000, 1000);
  const double c4Before = energyAt(3000, 2000, 261.63);

  noteOn(67 * 256);                          // the slur: no key released
  render(12000, 67 * 256);

  printf("period_before %.1f\n", before);
  printf("period_10ms %.1f\n", periodOf(544, 400));
  printf("period_20ms %.1f\n", periodOf(1024, 400));
  printf("period_after %.1f\n", periodOf(8000, 3000));
  printf("level_ratio %.3f\n", rms(5000, 1000) / (levelBefore > 1e-9f ? levelBefore : 1e-9f));
  printf("c4_before %.5f\n", c4Before);
  printf("c4_after %.5f\n", energyAt(8000, 2000, 261.63));
  printf("g4_after %.5f\n", energyAt(8000, 2000, 392.0));
  noteOff();
  return 0;
}
