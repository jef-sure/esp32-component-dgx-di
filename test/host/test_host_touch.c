/*
 * The touch of a window of DGX on the computer (dgx_di_host.h): the mouse pressed on a window is a finger of the
 * display at the pixel the window shows there, whichever way the board is held and the screen turned on it; let go,
 * no finger. The events are SDL's own, pushed as SDL gives them; no window opens (SDL_VIDEODRIVER=dummy).
 */
#include <SDL2/SDL.h>
#include <stdlib.h>

#include "check.h"
#include "dgx_di.h"
#include "dgx_di_host.h"
#include "dgx_host_screen.h"
#include "dgx_screen_with_bus.h"
#include "drivers/dgx_panel.h"
#include "esp_timer.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

/* the windows SDL has, in the order they were made */
static int windows(uint32_t *ids, int max)
{
    int n = 0;
    for (uint32_t id = 1; id < 256 && n < max; ++id)
        if (SDL_GetWindowFromID(id)) ids[n++] = id;
    return n;
}

/* the mouse on a window, at the middle of a pixel of what it shows (zoomed), as SDL gives it; the window then reads it */
static void mouse(uint32_t window, Uint32 type, int x, int y, int zoom)
{
    SDL_Event e = {0};
    e.type      = type;
    x = x * zoom + zoom / 2, y = y * zoom + zoom / 2;
    if (type == SDL_MOUSEMOTION) {
        e.motion.windowID = window, e.motion.x = x, e.motion.y = y, e.motion.state = SDL_BUTTON_LMASK;
    } else {
        e.button.windowID = window, e.button.button = SDL_BUTTON_LEFT, e.button.x = x, e.button.y = y, e.button.clicks = 1;
        e.button.state = type == SDL_MOUSEBUTTONDOWN ? SDL_PRESSED : SDL_RELEASED;
    }
    CHECK(SDL_PushEvent(&e) == 1);
    vTaskDelay(1);
}

/* the pointer read now: the first finger, or -1, -1 */
static void finger(dgx_di_pointer_t *p, int *x, int *y)
{
    dgx_di_pointer_read(p, (uint32_t)(esp_timer_get_time() / 1000));
    if (!dgx_di_pointer_first(p, x, y)) *x = -1, *y = -1;
}

int main(void)
{
    setenv("SDL_VIDEODRIVER", "dummy", 1);
    /* the JC4827W543: its glass straight; the CYD: its glass 240x320 upright, the board held turned, the screen turned on it */
    dgx_screen_t *jc  = dgx_host_screen_init(480, 272, 16, NULL);
    dgx_screen_t *cyd = dgx_host_screen_init(240, 320, 16, &(dgx_host_screen_options_t){.turn = 270});
    CHECK(jc && cyd);
    if (!jc || !cyd) CHECK_DONE();
    dgx_panel_orientation(cyd, DgxScreenRightLeft, DgxScreenTopBottom, true);
    vTaskDelay(1); /* the windows placed: their size */
    uint32_t ids[4];
    CHECK(windows(ids, 4) == 2);
    int w, h;
    SDL_GetWindowSize(SDL_GetWindowFromID(ids[0]), &w, &h);
    const int zoom = w / 480;
    CHECK(zoom >= 1 && h == 272 * zoom);
    SDL_GetWindowSize(SDL_GetWindowFromID(ids[1]), &w, &h);
    CHECK(w == 320 * zoom && h == 240 * zoom); /* the CYD's window: its glass turned */

    /* the device: the glass, whichever way the screen is turned when it is made */
    dgx_di_host_touch_t jc_touch, cyd_touch;
    dgx_di_host_touch_init(&jc_touch, jc);
    dgx_di_host_touch_init(&cyd_touch, cyd);
    CHECK(jc_touch.device.x_max == 479 && jc_touch.device.y_max == 271 && jc_touch.device.fingers == 1);
    CHECK(cyd_touch.device.x_max == 239 && cyd_touch.device.y_max == 319);

    dgx_di_pointer_t jp, cp;
    dgx_di_pointer_init(&jp, &jc_touch.device, 0, jc);
    dgx_di_pointer_init(&cp, &cyd_touch.device, 0, cyd);
    dgx_di_pointer_debounce(&jp, 0, 0);
    dgx_di_pointer_debounce(&cp, 0, 0);
    int x, y;

    /* straight: the pixel of the window is that of the display */
    finger(&jp, &x, &y);
    CHECK(x == -1);
    mouse(ids[0], SDL_MOUSEBUTTONDOWN, 100, 50, zoom);
    finger(&jp, &x, &y);
    CHECK(x == 100 && y == 50);
    finger(&cp, &x, &y);
    CHECK(x == -1); /* not on the other window */
    mouse(ids[0], SDL_MOUSEMOTION, 470, 260, zoom);
    finger(&jp, &x, &y);
    CHECK(x == 470 && y == 260);
    /* the screen turned upside down by DGX: the finger turns with it, the window shows the glass as it is */
    dgx_panel_orientation(jc, DgxScreenRightLeft, DgxScreenBottomTop, false);
    finger(&jp, &x, &y);
    CHECK(x == 479 - 470 && y == 271 - 260);
    mouse(ids[0], SDL_MOUSEBUTTONUP, 470, 260, zoom);
    finger(&jp, &x, &y);
    CHECK(x == -1);

    /* the CYD held turned, its screen turned on it as morph_demo of DGX does: upright in the window, and so is the finger */
    mouse(ids[1], SDL_MOUSEBUTTONDOWN, 10, 200, zoom);
    finger(&cp, &x, &y);
    CHECK(x == 10 && y == 200);
    mouse(ids[1], SDL_MOUSEMOTION, 300, 20, zoom);
    finger(&cp, &x, &y);
    CHECK(x == 300 && y == 20);
    mouse(ids[1], SDL_MOUSEBUTTONUP, 300, 20, zoom);
    finger(&cp, &x, &y);
    CHECK(x == -1);

    /* a screen that is not a window: no finger */
    dgx_di_point_t pts[DGX_DI_FINGERS];
    dgx_di_host_touch_t none = {.device = {.read = dgx_di_host_touch_read, .fingers = 1, .x_max = 7, .y_max = 7}, .screen = NULL};
    CHECK(none.device.read(&none.device, pts, DGX_DI_FINGERS) == 0);

    /* the screens and their windows let go of: the bus closes its window */
    dgx_bus_protocols_t *jc_bus = ((dgx_screen_with_bus_t *)jc)->bus, *cyd_bus = ((dgx_screen_with_bus_t *)cyd)->bus;
    dgx_screen_destroy(&jc);
    dgx_screen_destroy(&cyd);
    jc_bus->dispose(jc_bus);
    cyd_bus->dispose(cyd_bus);
    CHECK(windows(ids, 4) == 0);
    CHECK_DONE();
}
