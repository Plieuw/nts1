// Host cost of one unit, in nanoseconds per audio frame.
//
// The instrument is not this machine: a 3.5 GHz out-of-order core is nothing
// like a 180 MHz Cortex-M4, and the README says so where it quotes cycle counts
// from tools/cycles.py. What this measures is the *shape* of a change: the same
// workload, on the same host, before and after — which is how a unit is made
// lighter without guessing, and how one unit is compared to another.
//
//   tools/bench.cpp  built by tests/run.sh as:  bench --knobs N --ranges a:b,...
//
// Prints one line:  <unit> ns/frame  (and frames per second of one core).
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <ctime>
#include <cmath>

#include UNITFILE

static const int kBlock = 64;
static int gLo[8], gHi[8], gKnobs = 8;
static bool gSilent = false;  // the idle measurement: no input at all

static double now() {
  struct timespec ts;
  clock_gettime(CLOCK_MONOTONIC, &ts);
  return (double)ts.tv_sec + 1e-9 * (double)ts.tv_nsec;
}

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
  gParam.shape_lfo = 0;
  _hook_on(&gParam);
}
static void unitNoteOff() { _hook_off(&gParam); }
static void unitRender(int n) {
  for (int done = 0; done < n; done += kBlock) {
    int32_t b[kBlock];
    _hook_cycle(&gParam, b, kBlock);
  }
}
#else
static int gPos;
static float gSrc[4096];  // the input, worked out once so it is not part of the cost
static void unitInit() { _hook_init(0, 0x01010000); gPos = 0; }
static void unitParam(int index, int value) { _hook_param((uint8_t)index, (int32_t)value); }
static void unitNote(int) {}
static void unitNoteOff() {}
static float srcAt(int p) {
  if (p >= (int)(0.25 * 48000.0)) return 0.f;
  double v = (p == 0) ? 0.5 : 0.0;
  for (int i = 0; i < 48; ++i) {
    const double f = 80.0 * pow(2.0, i * (6.6 / 47.0));
    v += 0.006 * sin(2.0 * M_PI * f * p / 48000.0 + i * 1.7);
  }
  return (float)v;
}
static void unitRender(int n) {
  // A fixed source, so the input is identical in every run and in every unit.
  static float in[4096];
  for (int done = 0; done < n; done += kBlock) {
#if UNITKIND == 1
    float yl[kBlock], yr[kBlock];
    for (int i = 0; i < kBlock; ++i)
      in[i] = gSilent ? 0.f : gSrc[(gPos + i) & 4095];
    _hook_process(in, yl, in, yr, kBlock);
#else
    float io[2 * kBlock];
    for (int i = 0; i < kBlock; ++i) {
      const float v = gSilent ? 0.f : gSrc[(gPos + i) & 4095];
      io[2 * i] = io[2 * i + 1] = v;
    }
    _hook_process(io, kBlock);
#endif
    gPos += kBlock;
  }
}
#endif

int main(int argc, char **argv) {
#if UNITKIND != 0
  for (int i = 0; i < 4096; ++i) gSrc[i] = srcAt(i);
#endif
  for (int i = 0; i < 8; ++i) { gLo[i] = 0; gHi[i] = 1023; }
  for (int a = 1; a < argc; ++a) {
    if (strcmp(argv[a], "--knobs") == 0) { gKnobs = atoi(argv[++a]); continue; }
    if (strcmp(argv[a], "--idle") == 0) { gSilent = true; continue; }
    if (strcmp(argv[a], "--ranges") != 0) continue;
    const char *p = argv[a + 1];
    for (int i = 0; i < 8 && *p; ++i) {
      gLo[i] = (int)strtol(p, (char **)&p, 10);
      if (*p == ':') ++p;
      gHi[i] = (int)strtol(p, (char **)&p, 10);
      if (*p == ',') ++p;
    }
  }

  // Warm up, then time a long steady stretch: long enough that a scheduler tick
  // is noise, and with the unit in its steady state rather than its attack.
  for (int pass = 0; pass < 3; ++pass) {
    unitInit();
    for (int i = 0; i < gKnobs; ++i) unitParam(i, (gLo[i] + gHi[i]) / 2);
    unitNote(60);
    unitRender(48000);
    unitNoteOff();
  }

  const int frames = 48000 * 20;
  if (gSilent) {
    // Idle: an oscillator with no key down, an effect with no input. An effect
    // gets a second of signal first so its state is real, then a second to
    // settle, and only then is it timed on silence.
    unitInit();
    for (int i = 0; i < gKnobs; ++i) unitParam(i, (gLo[i] + gHi[i]) / 2);
#if UNITKIND == 0
    unitNoteOff();
#else
    gSilent = false;
    unitRender(48000);
    gSilent = true;
    unitRender(48000 * 3);
#endif
  } else {
  unitInit();
  for (int i = 0; i < gKnobs; ++i) unitParam(i, (gLo[i] + gHi[i]) / 2);
  unitNote(60);
  unitRender(4096);
  }
  // Three passes, keep the best: this host is shared with everything else on
  // the machine, so a single pass is a number and the minimum is a cost.
  double best = 1e30;
  for (int pass = 0; pass < 3; ++pass) {
    // Fresh strings for every pass: a resonator whose note has decayed would
    // otherwise be timed while it outputs zeros, which is not its cost.
    if (pass > 0) {
#if UNITKIND == 0
      unitInit();
      for (int i = 0; i < gKnobs; ++i) unitParam(i, (gLo[i] + gHi[i]) / 2);
      if (!gSilent) unitNote(60);
#else
      if (!gSilent) {
        unitInit();
        for (int i = 0; i < gKnobs; ++i) unitParam(i, (gLo[i] + gHi[i]) / 2);
        unitRender(48000);
      }
#endif
    }
    const double t0 = now();
    unitRender(frames);
    const double t1 = now();
    const double d = (t1 - t0) * 1e9 / (double)frames;
    if (d < best) best = d;
  }
  unitNoteOff();

  const double ns = best;
  printf("%s %.2f\n", gSilent ? "ns_per_frame_idle" : "ns_per_frame", ns);
  printf("core_fps %.0f\n", 1e9 / ns);
  return 0;
}
