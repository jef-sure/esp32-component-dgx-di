#pragma once

#include "dgx_di.h"
#include "dgx_di_pad.h"
#include "dgx_screen.h"

/*
 * What the check takes of a board, from its file (board_<name>.c, picked in menuconfig): its display, set up and
 * cleared, in its own orientation; its touch panel as a device of DGX-DI, its driver's mirroring and swapping left off
 * (the calibration finds how it lies); the display's size in millimetres, lying as it stands; a pad that fits it.
 */
extern const char board_name[];
extern const int  board_mm_w, board_mm_h;

/* the pad tried on it: the classic one (NULL) or the board's own, when the classic does not fit the display */
extern const dgx_di_pad_item_t *const board_pad;
extern const int                      board_pad_count;

/* its calibration measured by this check, pasted from the log (NULL: none yet, the check measures it first) */
extern const dgx_di_calibration_t *const board_calibration;

/* the display: NULL when it cannot be set up */
dgx_screen_t *board_display(void);

/* the touch panel: NULL when it does not answer */
dgx_di_device_t *board_touch(void);
