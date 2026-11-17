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

// The cosine alone, for the places where only that is wanted.
inline float arc_cosf(float w) {
  float s, c;
  arc_sincos(w, s, c);
  return c;
}

// sqrt, from a bit pattern and three Newton steps: about 1e-7 relative, which is
// all a filter coefficient or a level trim needs. The divide is a VDIV (14
// cycles) and this is only ever called when a knob moves.
inline float arc_sqrtf(float x) {
  if (x <= 0.f)
    return 0.f;
  union { float f; uint32_t u; } v;
  v.f = x;
  v.u = (v.u >> 1) + 0x1fc00000u;
  float y = v.f;
  y = 0.5f * (y + x / y);
  y = 0.5f * (y + x / y);
  y = 0.5f * (y + x / y);
  return y;
}

// 10^(db / 20), the shape every level trim here is written in.
inline float arc_dbtolevel(float db) {
  return arc_pow2f(db * 0.1660964f);   // 1/20 * log2(10)
}
