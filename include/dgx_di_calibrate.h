#pragma once

#include <stdbool.h>

#include "dgx_di.h"
#include "dgx_screen.h"

#ifdef __cplusplus
extern "C" {
#endif

/*
 * The calibration of a board measured, once: not the program but a check the developer runs on the board (before the
 * program goes out, say), its result to paste into the program or keep (dgx_di.h: dgx_di_calibration_t). Nothing of
 * the program's is taken: the device is read as it is, its flags found, so it works on a board never seen.
 *
 * A target blinks on the display in turn — the corners, the middles of the edges, the middle — and a finger goes down
 * on it and stays a moment: a finger already down does not count, it has to come down on the target. From the second
 * round on, a thin dot where the panel sees a finger not yet pressed and a thick ring where the press is taken, by the
 * calibration of the rounds before; the log tells how far off. After each round of nine: how far off on average and
 * at worst, and the calibration of the rounds so far (each target where the median of its touches has it, of the last
 * 15 rounds: a touch beside the target does not move it), in words and as C to paste; then round again. Near the glass
 * a panel shows a finger before it is pressed, held it loses one now and then: the longest of each makes how it is
 * steadied.
 *
 * Blocks, round after round: call it from the task that owns the display, nothing else drawing meanwhile; it takes some
 * 3 KB of that task's stack (the ESP32's main task has 3.5 by default: give it more) and 2 KB of the heap. rounds: so
 * many, then the calibration of them all into *calibration and true (false: no device or no memory); 0: rounds without
 * end, logged.
 */
bool dgx_di_calibrate_measure(dgx_di_device_t *device, dgx_screen_t *screen, int rounds, dgx_di_calibration_t *calibration);

#ifdef __cplusplus
}
#endif
