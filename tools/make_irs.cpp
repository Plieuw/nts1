/*
 * make_irs.cpp -- writes the body unit's impulse responses as WAV files.
 *
 * Build and run from the repository root:
 *
 *   g++ -O2 tools/make_irs.cpp -o /tmp/make_irs && /tmp/make_irs ir
 *
 * The responses come from units/body/src/ir_model.h, the same header the unit
 * renders from on the device, so these files are exactly what it convolves
 * with. Mono, 48 kHz, 16 bit PCM.
 */

#include <cstdio>
#include <cstring>
#include <string>
#include <vector>

#include "../units/body/src/ir_model.h"

namespace {

void writeLe16(FILE *f, unsigned v) {
  fputc(v & 0xFF, f);
  fputc((v >> 8) & 0xFF, f);
}

void writeLe32(FILE *f, unsigned v) {
  writeLe16(f, v & 0xFFFF);
  writeLe16(f, (v >> 16) & 0xFFFF);
}

bool writeWav(const std::string &path, const std::vector<float> &samples,
              int sample_rate) {
  FILE *f = fopen(path.c_str(), "wb");
  if (!f)
    return false;

  const unsigned data_bytes = (unsigned)(samples.size() * 2);

  fwrite("RIFF", 1, 4, f);
  writeLe32(f, 36 + data_bytes);
  fwrite("WAVE", 1, 4, f);

  fwrite("fmt ", 1, 4, f);
  writeLe32(f, 16);
  writeLe16(f, 1);  // PCM
  writeLe16(f, 1);  // mono
  writeLe32(f, (unsigned)sample_rate);
  writeLe32(f, (unsigned)sample_rate * 2);
  writeLe16(f, 2);  // block align
  writeLe16(f, 16); // bits

  fwrite("data", 1, 4, f);
  writeLe32(f, data_bytes);

  for (float s : samples) {
    float v = s;
    if (v > 1.f)
      v = 1.f;
    if (v < -1.f)
      v = -1.f;
    const int q = (int)(v * 32767.f);
    writeLe16(f, (unsigned)(q & 0xFFFF));
  }

  fclose(f);
  return true;
}

}  // namespace

int main(int argc, char **argv) {
  const std::string dir = (argc > 1) ? argv[1] : "ir";
  const int fs = (int)irmodel::kFs;

  printf("%-8s %8s %8s %9s %9s\n", "preset", "peak", "rms", "-40dB", "-60dB");

  for (int i = 0; i < irmodel::kPresetCount; ++i) {
    std::vector<float> h((size_t)irmodel::kIrLen, 0.f);
    irmodel::render(h.data(), i);

    const std::string path = dir + "/" + irmodel::kPresets[i].name + ".wav";
    if (!writeWav(path, h, fs)) {
      fprintf(stderr, "cannot write %s\n", path.c_str());
      return 1;
    }

    double peak = 0.0, energy = 0.0;
    int last_40 = 0, last_60 = 0;
    for (int n = 0; n < irmodel::kIrLen; ++n) {
      const double v = (double)h[n];
      const double a = v < 0 ? -v : v;
      if (a > peak)
        peak = a;
      energy += v * v;
      if (a > 0.01)
        last_40 = n;  // -40 dBFS
      if (a > 0.001)
        last_60 = n;  // -60 dBFS
    }

    printf("%-8s %8.4f %8.5f %7.1fms %7.1fms  %s\n", irmodel::kPresets[i].name,
           peak, sqrt(energy / irmodel::kIrLen), 1000.0 * last_40 / fs,
           1000.0 * last_60 / fs, path.c_str());
  }

  return 0;
}
