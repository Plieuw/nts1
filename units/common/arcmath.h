#pragma once
// Small, libm free maths, shared by the units that need it.
//
// A single call into the maths library is enough for the linker to pull
// newlib's reentrancy struct into the image: about a kilobyte of RAM and a few
// kilobytes of code, out of the 6 or 12 KB a slot gets. Every unit therefore
// does its own exponentials and sines with a short polynomial, and this is where
// that polynomial lives.
//
// The units include it by relative path, so no build file has to know about it.

#include <stdint.h>

// 2^x, to about 1e-6: the exponent is split off as a float bit pattern and the
// fraction is a short series.
inline float arc_pow2f(float x) {
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

// e^-a for a small, positive a: what a decay coefficient needs.
inline float arc_expneg(float a) {
  return 1.f - a + a * a * (0.5f - a * 0.16666667f);
}

// sin and cos of a radian angle below 2.9, which is all a mode or body
// frequency can produce here (they stop at 0.45 of the sample rate).
inline void arc_sincos(float w, float &s, float &c) {
  const float w2 = w * w;
  s = w * (1.f + w2 * (-1.66666667e-1f + w2 * (8.33333333e-3f +
      w2 * (-1.98412698e-4f + w2 * 2.75573192e-6f))));
  c = 1.f + w2 * (-5e-1f + w2 * (4.16666667e-2f + w2 * (-1.38888889e-3f +
      w2 * 2.48015873e-5f)));
}
