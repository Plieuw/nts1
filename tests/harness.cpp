// Host harness: builds one unit against tests/host and measures what comes out.
//
// Compiled with UNITFILE (the unit's source) and UNITKIND (0 = osc, 1 = modfx,
// 2 = delfx, 3 = revfx). Prints one metric per line so tests/run.sh can compare
// it against tests/golden, and so a human can read what a change did to the
// sound without an instrument on the desk.
//
// tests/run.sh passes --ranges "lo:hi,..." with the knob ranges from the unit's
// manifest: the knobs do not all run 0..1023 (a model index is 0..16, some are
// bipolar percentages), so the harness cannot guess them.
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <cmath>

#include UNITFILE

static const int kBlock = 64;
static const int kSamples = 48000 * 2;
static const double kRate = 48000.0;
static float gOut[kSamples];
static int gLo[8], gHi[8], gKnobs = 8, gModels = 0;

// ------------------------------------------------------------------ the unit

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
static void unitNoteOff() { _hook_off(&gParam); }
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
static void unitNote(int) {}
static void unitNoteOff() {}
// A tone plus a click, so a gate, a resonator, a delay and a reverb all get
// something they can answer.
// Broadband on purpose: a single tone hides a phaser's notches and a gate's
// timbre, a spread of tones shows them.
static float srcAt(int p) {
  if (p >= (int)(0.25 * kRate)) return 0.f;
  // Log-spaced tones: a dozen of them would leave the gaps a phaser's notches
  // fall into invisible.
  double v = (p == 0) ? 0.5 : 0.0;
  for (int i = 0; i < 48; ++i) {
    const double f = 80.0 * pow(2.0, i * (6.6 / 47.0));  // 80 Hz .. 7.8 kHz
    v += 0.006 * sin(2.0 * M_PI * f * p / kRate + i * 1.7);
  }
  return (float)v;
}
static void unitRender(float *out, int n) {
  int done = 0;
  while (done < n) {
#if UNITKIND == 1
    // Modulation effects take two mono inputs (the two timbres) and return two.
    float in[kBlock], yl[kBlock], yr[kBlock];
    for (int i = 0; i < kBlock; ++i) in[i] = srcAt(gPos + i);
    _hook_process(in, yl, in, yr, kBlock);
    for (int i = 0; i < kBlock && done < n; ++i) out[done++] = 0.5f * (yl[i] + yr[i]);
#else
    // Delay and reverb buffers hold frames of two interleaved samples.
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

// ---------------------------------------------------------------- measurement

// Energy in third-octave bands, 50 Hz to 12 kHz, as dB relative to the loudest
// band: a shape of the sound, which is what a timbre regression shows up in.
static const int kBands = 28;
static void spectrum(const float *x, int n, double *db) {
  double e[kBands], emax = 1e-20;
  for (int b = 0; b < kBands; ++b) {
    const double f = 50.0 * pow(2.0, b / 3.0);
    const double w = 2.0 * M_PI * f / kRate;
    double re = 0, im = 0;
    for (int i = 0; i < n; ++i) {
      const double win = 0.5 - 0.5 * cos(2.0 * M_PI * i / (n - 1));
      re += x[i] * win * cos(w * i);
      im -= x[i] * win * sin(w * i);
    }
    e[b] = re * re + im * im;
    if (e[b] > emax) emax = e[b];
  }
  for (int b = 0; b < kBands; ++b) db[b] = 10.0 * log10(e[b] / emax + 1e-30);
}

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

static int nanCount(const float *x, int n) {
  int c = 0;
  for (int i = 0; i < n; ++i)
    if (!(x[i] == x[i]) || fabs(x[i]) > 1e6) ++c;
  return c;
}

// Two windows: how the sound starts and how it carries on. A gate or a delay
// shows its time knob only in the second one, a strung or struck body shows its
// brightness only in the first.
static int windowAttack() { return (int)((UNITKIND == 0 ? 0.01 : 0.05) * kRate); }
static int windowTail() { return (int)((UNITKIND == 0 ? 0.25 : 0.30) * kRate); }
static const int kWindow = 4096;

static void reportBands(const char *tag, const float *x, int at) {
  double db[kBands];
  spectrum(x + at, kWindow, db);
  printf("%s", tag);
  for (int b = 0; b < kBands; ++b) printf(" %.1f", db[b]);
  printf("\n");
}

// ------------------------------------------------------------------ scenarios

static void renderInto(float *out, int n) {
  for (int i = 0; i < n; ++i) out[i] = 0.f;
  unitRender(out, n);
}

static void setNominal() {
  for (int i = 0; i < gKnobs; ++i) unitParam(i, (gLo[i] + gHi[i]) / 2);
}

static void runNominal(int knob = -1, int value = 0, int knob2 = -1, int value2 = 0) {
  unitInit();
  setNominal();
  if (knob >= 0) unitParam(knob, value);
  if (knob2 >= 0) unitParam(knob2, value2);
  unitNote(60);
  renderInto(gOut, kSamples);
}

int main(int argc, char **argv) {
  for (int i = 0; i < 8; ++i) {
    gLo[i] = 0;
    gHi[i] = 1023;
  }
  for (int a = 1; a + 1 < argc; ++a) {
    if (strcmp(argv[a], "--knobs") == 0) { gKnobs = atoi(argv[++a]); continue; }
    if (strcmp(argv[a], "--permodel") == 0) { gModels = atoi(argv[++a]); continue; }
    if (strcmp(argv[a], "--ranges") != 0) continue;
    const char *p = argv[a + 1];
    for (int i = 0; i < 8 && *p; ++i) {
      gLo[i] = (int)strtol(p, (char **)&p, 10);
      if (*p == ':') ++p;
      gHi[i] = (int)strtol(p, (char **)&p, 10);
      if (*p == ',') ++p;
    }
  }

  runNominal();
  printf("rms %.4f\n", rmsOf(gOut + windowAttack(), kWindow));
  printf("rms_tail %.4f\n", rmsOf(gOut + windowTail(), kWindow));
  printf("rms_late %.4f\n", rmsOf(gOut + (int)(1.2 * kRate), kWindow));
  printf("peak %.3f\n", peakOf(gOut, kSamples));
  printf("nan %d\n", nanCount(gOut, kSamples));
  reportBands("bands", gOut, windowAttack());
  reportBands("bands_tail", gOut, windowTail());
  unitNoteOff();

  // Every knob has to do something: the biggest band that moves when it goes
  // from the middle to the top of its range.
  double baseA[kBands], baseT[kBands];
  spectrum(gOut + windowAttack(), kWindow, baseA);
  spectrum(gOut + windowTail(), kWindow, baseT);
  const double gBaseA = rmsOf(gOut + windowAttack(), kWindow);
  const double gBaseT = rmsOf(gOut + windowTail(), kWindow);
  const double gBaseL = rmsOf(gOut + (int)(1.2 * kRate), kWindow);
  for (int k = 0; k < gKnobs; ++k) {
    double dbA[kBands], dbT[kBands], worst = 0;
    runNominal(k, gHi[k]);
    spectrum(gOut + windowAttack(), kWindow, dbA);
    spectrum(gOut + windowTail(), kWindow, dbT);
    for (int b = 0; b < kBands; ++b) {
      if (baseA[b] > -40.0 && fabs(dbA[b] - baseA[b]) > worst) worst = fabs(dbA[b] - baseA[b]);
      if (baseT[b] > -40.0 && fabs(dbT[b] - baseT[b]) > worst) worst = fabs(dbT[b] - baseT[b]);
    }
    // A decay or a level knob moves the amount, not the shape, so the level of
    // both windows counts too.
    const double la = rmsOf(gOut + windowAttack(), kWindow);
    const double lt = rmsOf(gOut + windowTail(), kWindow);
    const double ll = rmsOf(gOut + (int)(1.2 * kRate), kWindow);
    const double ra = fabs(20.0 * log10((la + 1e-9) / (gBaseA + 1e-9)));
    const double rt = fabs(20.0 * log10((lt + 1e-9) / (gBaseT + 1e-9)));
    const double rl = fabs(20.0 * log10((ll + 1e-9) / (gBaseL + 1e-9)));
    if (ra > worst) worst = ra;
    if (rt > worst) worst = rt;
    if (rl > worst) worst = rl;
    unitNoteOff();
    printf("knob%d %.1f\n", k, worst);
  }

  // An oscillator's A and B knobs are per model: the sweep above only proves
  // they move on the model in the middle. This asks every model.
  if (gModels > 0) {
    for (int m = 0; m < gModels; ++m) {
      double hi[kBands], lo[kBands], worst = 0;
      static const double kAt[4] = {0.01, 0.06, 0.15, 0.40};
      for (int pass = 0; pass < 4; ++pass) {
        const int at = (int)(kAt[pass] * kRate);
        runNominal(0, gLo[0] + m, 7, gHi[7]);
        spectrum(gOut + at, kWindow, hi);
        unitNoteOff();
        runNominal(0, gLo[0] + m, 7, gLo[7]);
        spectrum(gOut + at, kWindow, lo);
        const double level = rmsOf(gOut + at, kWindow);
        unitNoteOff();
        if (level < 1e-4) continue;  // that model has already finished
        for (int b = 0; b < kBands; ++b)
          if (hi[b] > -40.0 && fabs(hi[b] - lo[b]) > worst) worst = fabs(hi[b] - lo[b]);
      }
      printf("model%d %.1f\n", m, worst);
    }
  }

  // Nothing may blow up under abuse: random parameters while a note rings.
  unitInit();
  setNominal();
  unitNote(48);
  {
    srand(12345);
    double pk = 0;
    int nan = 0;
    for (int step = 0; step < 2000; ++step) {
      const int k = rand() % (gKnobs > 0 ? gKnobs : 1);
      unitParam(k, gLo[k] + rand() % (gHi[k] - gLo[k] + 1));
      if (step % 37 == 0) unitNote(24 + rand() % 76);
      renderInto(gOut, kBlock * 2);
      pk = fmax(pk, peakOf(gOut, kBlock * 2));
      nan += nanCount(gOut, kBlock * 2);
    }
    printf("fuzzpeak %.3f\n", pk);
    printf("fuzznan %d\n", nan);
  }
  return 0;
}
