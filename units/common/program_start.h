/*
 * program_start.h -- the values a program hands a freshly installed unit.
 *
 * BSD 3-Clause License, see LICENSE.
 *
 * A program stores one value per parameter of the unit in its slot, and for a
 * program that has never held this unit those values are all zero. They are not
 * the unit's start values: those are what the unit's INIT sets up. And zero is
 * a level of zero, or a wet amount of nothing, so a unit that believed the
 * program opened its own output and looked dead on the instrument until a knob
 * was moved. That is not a theory: `noise` and `bowed` were silent on the NTS-1
 * and `resonator` only sounded after its Level was turned.
 *
 * So the first parameter value decides. While every value received has been
 * zero, the unit keeps what its INIT set; from the first value that is not zero
 * on, the program is in charge, zeroes included, and a knob turned all the way
 * down means what it says. A unit therefore sounds the way it was designed to
 * the moment it is installed, and a patch saved with real settings wins as soon
 * as it has any.
 *
 * The A and B knobs are a signal of their own. Their zero is not a silence (on
 * `bowed` it is a bow with no pressure, which is close enough to one), so they
 * are latched separately: a program that has never been set up for this unit
 * leaves them at the unit's own positions too, and the first A/B value the user
 * actually chooses takes over without dragging the level knobs with it.
 */
#ifndef NTS1_PROGRAM_START_H
#define NTS1_PROGRAM_START_H

#include <stdint.h>

namespace program_start {

// One flag per unit, in that unit's own translation unit.
static bool g_knobs = false;  // the level, model and amount knobs
static bool g_shape = false;  // the A and B knobs

// True once a value that is not zero has been seen; false while the only values
// that arrived were zeroes, which is the state the unit should ignore.
inline bool chosen(int32_t value) {
  if (!g_knobs && value != 0)
    g_knobs = true;
  return g_knobs;
}

inline bool chosen_shape(int32_t value) {
  if (!g_shape && value != 0)
    g_shape = true;
  return g_shape;
}

// Called from the unit's INIT, so a unit that is instantiated again starts from
// its own values rather than from a patch that is no longer loaded.
inline void reset() {
  g_knobs = false;
  g_shape = false;
}

}  // namespace program_start

#endif
