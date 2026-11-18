/*
 * silence.h -- is this block digital silence?
 *
 * BSD 3-Clause License, see LICENSE.
 *
 * An effect slot is handed the synth's output, and with no key down that is
 * exactly zero, sample after sample. A unit whose input is all zeroes and whose
 * own state has run down to nothing has nothing to compute: every term of its
 * sum is zero, so its output is zero. Writing that and skipping the DSP is not
 * an approximation, and it is where most of an effect's cost can go: on an
 * instrument that is idle between notes, the slots used to run flat out.
 *
 * Only exact zero counts. A denormal or a -300 dB tail is real state and has to
 * be processed (which is what the settle guards in the units are for: they zero
 * a state that has decayed past anything audible, and after that this can match).
 */
#ifndef NTS1_SILENCE_H
#define NTS1_SILENCE_H

#include <stdint.h>

namespace silence {

inline bool block(const float *x, uint32_t frames) {
  for (uint32_t i = 0; i < frames; ++i)
    if (x[i] != 0.f)
      return false;
  return true;
}

// Interleaved stereo, both channels.
inline bool block(const float *x, uint32_t frames, int channels) {
  const uint32_t n = frames * (uint32_t)channels;
  for (uint32_t i = 0; i < n; ++i)
    if (x[i] != 0.f)
      return false;
  return true;
}

}  // namespace silence

#endif
