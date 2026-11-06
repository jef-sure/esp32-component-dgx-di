#pragma once

#include "dgx_di.h"
#include "dgx_host_screen.h"

#ifdef __cplusplus
extern "C" {
#endif

/*
 * The touch of a window of DGX on the computer (dgx_host_screen.h, DGX 0.10.10 and later) as a device of DGX-DI: the
 * mouse, while its left button is held after it went down on the window, is a finger on the glass. For a program built
 * for the computer only, where DGX defines DGX_HOST_WINDOW: with make (host/dgx_host.mk of DGX, host/dgx_di_host.mk of
 * DGX-DI) or by ESP-IDF for its linux target. All of it static inline: compiled only where it is included.
 *
 * Its points are pixels of the glass upright, as a touch panel under it would give them: the pointer lies by flags 0
 * and turns them as the screen of DGX is turned, as on a board; the mouse needs no calibration. One finger: the window
 * gives no second one yet.
 */
typedef struct {
    dgx_di_device_t     device; /* what a pointer reads: dgx_di_pointer_init(&pointer, &touch.device, 0, scr) */
    const dgx_screen_t *screen;
} dgx_di_host_touch_t;

/* the fingers on the window now: so many, at most max (a dgx_di_read_fn) */
static inline int dgx_di_host_touch_read(dgx_di_device_t *device, dgx_di_point_t *points, int max)
{
    dgx_di_host_touch_t *t = (dgx_di_host_touch_t *)device;
    dgx_host_finger_t    fingers[DGX_DI_FINGERS];
    if (max > DGX_DI_FINGERS) max = DGX_DI_FINGERS;
    const int n = max > 0 ? dgx_host_screen_fingers(t->screen, fingers, max) : 0;
    for (int i = 0; i < n; ++i) points[i] = (dgx_di_point_t){.x = fingers[i].x, .y = fingers[i].y, .size = 0, .id = fingers[i].id};
    return n;
}

/* The touch of the window of a screen made by dgx_host_screen_init(): its range the glass, whichever way the screen is turned now. */
static inline void dgx_di_host_touch_init(dgx_di_host_touch_t *t, const dgx_screen_t *screen)
{
    const int w = screen->swap_xy ? screen->height : screen->width, h = screen->swap_xy ? screen->width : screen->height;
    t->screen   = screen;
    t->device   = (dgx_di_device_t){.read = dgx_di_host_touch_read, .fingers = 1, .x_min = 0, .x_max = (int16_t)(w - 1), .y_min = 0,
                                    .y_max = (int16_t)(h - 1)};
}

#ifdef __cplusplus
}
#endif
