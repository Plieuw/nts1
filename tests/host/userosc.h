#pragma once
#include "logue_host.h"
typedef struct user_osc_param { int32_t shape_lfo; uint16_t pitch; uint16_t cutoff; uint16_t resonance; uint16_t reserved0[3]; } user_osc_param_t;
enum { k_user_osc_param_id1=0, k_user_osc_param_id2, k_user_osc_param_id3, k_user_osc_param_id4,
       k_user_osc_param_id5, k_user_osc_param_id6, k_user_osc_param_shape, k_user_osc_param_shiftshape };
#define OSC_INIT _hook_init
#define OSC_CYCLE _hook_cycle
#define OSC_NOTEON _hook_on
#define OSC_NOTEOFF _hook_off
#define OSC_MUTE _hook_mute
#define OSC_PARAM _hook_param
