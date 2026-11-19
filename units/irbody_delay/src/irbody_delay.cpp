/*
 * irbody_delay.cpp -- irbody, built for the delay slot.
 *
 * The DSP is the reverb unit's, unchanged: same bodies, same knobs, same
 * output. What differs is the module type, which is what decides the slot the
 * unit lands in and the magic in its hook table -- a delay unit and a reverb
 * unit have the same hooks, so the source is included rather than copied and
 * the two builds cannot drift apart.
 *
 * Why it exists: on the mkI a custom delay and a custom reverb are loaded into
 * the same runtime, so `barber` and `irbody` cannot be enabled together. With
 * the body in the delay slot, a stock reverb can sit after it.
 *
 * The knobs it answers to, under the delay slot's own names:
 * k_user_delfx_param_time, k_user_delfx_param_depth and
 * k_user_delfx_param_shift_depth.
 */

#define NTS1_DELAY 1
#include "../../irbody/src/irbody.cpp"
