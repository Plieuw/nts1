#pragma once
// A small stand-in for the parts of the NTS-1 SDK the units touch, so they can
// be compiled and measured on a PC instead of only on the instrument. It is not
// the SDK: the API names, the parameter scaling and the pitch helpers follow
// it, the DSP helpers are plain libm. Anything that behaves differently here
// than on the device (osc_sinf and osc_white especially) is a test artefact.
#include <stdint.h>
#include <math.h>
#include <stdlib.h>
typedef int32_t q31_t;
#define k_samplerate 48000
#define __sdram
static inline float clipminmaxf(float mn, float x, float mx){ return x<mn?mn:(x>mx?mx:x); }
static inline float clip1f(float x){ return clipminmaxf(0.f,x,1.f); }
static inline float clip1m1f(float x){ return clipminmaxf(-1.f,x,1.f); }
static inline float si_fabsf(float x){ return fabsf(x); }
static inline float linintf(float fr, float x0, float x1){ return x0 + fr*(x1-x0); }
static inline q31_t f32_to_q31(float x){ float y = clip1m1f(x); return (q31_t)(y*2147483647.f); }
static inline float q31_to_f32(q31_t x){ return (float)x * (1.f/2147483647.f); }
static inline float fx_softclipf(float c, float x){ x = clip1m1f(x); return x - c*x*x*x; }
static inline float osc_softclipf(float c, float x){ return fx_softclipf(c,x); }
static inline float fx_tanpif(float x){ return tanf(3.14159265358979f*x); }
static inline float fx_pow2f(float x){ return powf(2.f,x); }
static inline float fx_sinf(float x){ return sinf(6.283185307179586f*x); }
static inline float fx_cosf(float x){ return cosf(6.283185307179586f*x); }
static inline float osc_sinf(float x){ return fx_sinf(x); }
static inline float osc_cosf(float x){ return fx_cosf(x); }
static inline float osc_tanpif(float x){ return fx_tanpif(x); }
static inline float osc_white(){ return (float)rand()/(float)RAND_MAX*2.f-1.f; }
static inline float fx_white(){ return osc_white(); }
static inline uint32_t osc_mcu_hash(){ return 0x5A5A5A5Au; }
static inline float osc_notehzf(uint8_t n){ return 440.f*powf(2.f,((float)n-69.f)/12.f); }
static inline float osc_w0f_for_note(uint8_t n, uint8_t m){
  const float f0 = osc_notehzf(n), f1 = osc_notehzf(n+1);
  return (f0 + ((float)m/255.f)*(f1-f0)) / 48000.f; }
static inline float param_val_to_f32(uint16_t v){ return (float)v * 9.77517106549365e-004f; }
