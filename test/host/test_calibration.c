/*
 * A calibration made from targets: a device made up to lie under a display each of the 8 ways, its touches skewed and
 * an edge pressed together as a panel's are; the flags found, the targets where they were meant after it; what it says
 * in words and as C. The display 800x1280 in its own orientation, a device of 1664x896 (the 10" Guition's GSL3680).
 */
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "check.h"
#include "dgx_di.h"

static int fake_read(dgx_di_device_t *device, dgx_di_point_t *points, int max)
{
    return 0;
}

static dgx_di_device_t device = {.read = fake_read, .fingers = 10, .x_min = 0, .x_max = 1663, .y_min = 0, .y_max = 895};
enum { W = 800, H = 1280 };

/* how the made-up panel errs: where it takes a touch meant at x, y (pixels of the display) */
typedef void (*err_fn)(double x, double y, double *ex, double *ey);

static void exact(double x, double y, double *ex, double *ey)
{
    *ex = x, *ey = y;
}

/* skewed, shifted, stretched: what an affine mends */
static void skewed(double x, double y, double *ex, double *ey)
{
    *ex = 1.02 * x + 0.03 * y - 12, *ey = 0.97 * y - 0.02 * x + 25;
}

/* and pressed together near its bottom: what bends */
static void bent(double x, double y, double *ex, double *ey)
{
    skewed(x, y, ex, ey);
    if (y > 900) *ey -= (y - 900) * 0.08;
}

/* the device's point a panel lying as flags say takes for the display's x, y (as the pointer maps, backwards) */
static void device_point(int flags, double x, double y, int16_t *dx, int16_t *dy)
{
    const int rx = device.x_max - device.x_min + 1, ry = device.y_max - device.y_min + 1;
    int       w = rx, h = ry;
    if (flags & DGX_DI_SWAP_XY) w = ry, h = rx;
    double a = (x + 0.5) * w / W - 0.5, b = (y + 0.5) * h / H - 0.5;
    if (flags & DGX_DI_SWAP_XY) {
        const double t = a;
        a = b, b = t;
    }
    if (flags & DGX_DI_MIRROR_X) a = rx - 1 - a;
    if (flags & DGX_DI_MIRROR_Y) b = ry - 1 - b;
    *dx = (int16_t)lround(a + device.x_min), *dy = (int16_t)lround(b + device.y_min);
}

/* nine targets, the corners, the middles of the edges and the middle, 48 in from the edges */
static int targets(dgx_di_aim_t *aims, int flags, err_fn err)
{
    static const int xs[3] = {48, W / 2, W - 1 - 48}, ys[3] = {48, H / 2, H - 1 - 48};
    int              n     = 0;
    for (int j = 0; j < 3; ++j)
        for (int i = 0; i < 3; ++i, ++n) {
            double ex, ey;
            err(xs[i], ys[j], &ex, &ey);
            aims[n].meant_x = (int16_t)xs[i], aims[n].meant_y = (int16_t)ys[j];
            device_point(flags, ex, ey, &aims[n].x, &aims[n].y);
        }
    return n;
}

/* the worst a pointer by c takes points meant over the display between the targets */
static int worst_between(const dgx_di_calibration_t *c, int flags, err_fn err)
{
    static const dgx_screen_t s = {.width = W, .height = H, .dir_x = DgxScreenLeftRight, .dir_y = DgxScreenTopBottom};
    dgx_di_pointer_t          p;
    dgx_di_pointer_init(&p, &device, 0, &s);
    if (!dgx_di_pointer_calibrate(&p, c)) return 9999;
    int worst = 0;
    for (int y = 48; y <= H - 49; y += 37)
        for (int x = 48; x <= W - 49; x += 29) {
            double  ex, ey;
            int16_t dx, dy;
            int     mx, my;
            err(x, y, &ex, &ey);
            device_point(flags, ex, ey, &dx, &dy);
            dgx_di_pointer_map(&p, dx, dy, &mx, &my, NULL);
            const int o = abs(mx - x) > abs(my - y) ? abs(mx - x) : abs(my - y);
            worst = o > worst ? o : worst;
        }
    return worst;
}

int main(void)
{
    dgx_di_aim_t         aims[16];
    dgx_di_calibration_t c;

    /*
     * each of the 8 ways a panel may lie, its touches exact: the flags found, nothing off but the rounding — the device's
     * step is coarser than a pixel along one way or the other (896 over 1280: 1.4 pixels), so within 2
     */
    for (int flags = 0; flags < 8; ++flags) {
        const int n = targets(aims, flags, exact);
        CHECK(dgx_di_calibration_make(&c, &device, W, H, aims, n));
        if (c.flags != flags) fprintf(stderr, "lying %d: found %d\n", flags, c.flags);
        CHECK(c.flags == flags);
        CHECK(c.worst_tenths <= 10 && c.left_worst_tenths <= 10);
        const int between = worst_between(&c, flags, exact);
        if (between > 2) fprintf(stderr, "lying %d: %d off between the targets\n", flags, between);
        CHECK(between <= 2);
        CHECK(c.version == DGX_DI_CALIBRATION_VERSION && c.size == sizeof c && c.width == W && c.height == H && c.x_max == 1663 && c.y_max == 895);
    }

    /* skewed: off by tens of pixels without it, within a pixel or so with it at the targets, within 3 between them */
    for (int flags = 0; flags < 8; ++flags) {
        const int n = targets(aims, flags, skewed);
        CHECK(dgx_di_calibration_make(&c, &device, W, H, aims, n) && c.flags == flags);
        CHECK(c.worst_tenths >= 200);
        CHECK(c.left_worst_tenths <= 10);
        const int between = worst_between(&c, flags, skewed);
        if (between > 3) fprintf(stderr, "skewed %d: %d off between the targets\n", flags, between);
        CHECK(between <= 3);
    }

    /* and its bottom pressed together: the targets where meant, between them nearer than without the points */
    {
        const int flags = DGX_DI_SWAP_XY | DGX_DI_MIRROR_Y, n = targets(aims, flags, bent);
        CHECK(dgx_di_calibration_make(&c, &device, W, H, aims, n) && c.flags == flags);
        CHECK(c.knots_x == 3 && c.knots_y == 3);
        CHECK(c.left_worst_tenths <= 10);
        dgx_di_calibration_t affine_alone = c;
        affine_alone.knots_x = affine_alone.knots_y = 0;
        const int with = worst_between(&c, flags, bent), without = worst_between(&affine_alone, flags, bent);
        if (with >= without) fprintf(stderr, "bent: %d off with the points, %d without\n", with, without);
        CHECK(with < without);
    }

    /* how far off, exactly: a device of the display's own pixels, its touches 0, 1, 2 and 3 off at the corners */
    {
        dgx_di_device_t d = {.read = fake_read, .fingers = 1, .x_min = 0, .x_max = W - 1, .y_min = 0, .y_max = H - 1};
        dgx_di_aim_t    four[4] = {{48, 48, 48, 48}, {751, 48, 752, 48}, {48, 1231, 48, 1229}, {751, 1231, 754, 1231}};
        CHECK(dgx_di_calibration_make(&c, &d, W, H, four, 4) && c.flags == 0);
        CHECK(c.off_tenths == 15 && c.worst_tenths == 30);
    }

    /* targets that do not span the display: too few, in a line */
    {
        const int n = targets(aims, 0, exact);
        CHECK(!dgx_di_calibration_make(&c, &device, W, H, aims, 3));
        for (int i = 0; i < n; ++i) aims[i].meant_y = 640, aims[i].y = 448;
        CHECK(!dgx_di_calibration_make(&c, &device, W, H, aims, n));
        CHECK(!dgx_di_calibration_make(&c, &device, 0, H, aims, n));
    }

    /* more places along an axis than points: no points on it, the affine still */
    {
        int n = 0;
        for (int j = 0; j < 4; ++j)
            for (int i = 0; i < 3; ++i, ++n) {
                const int x = (int[]){48, 400, 751}[i], y = 48 + j * 390;
                double    ex, ey;
                skewed(x, y, &ex, &ey);
                aims[n].meant_x = (int16_t)x, aims[n].meant_y = (int16_t)y;
                device_point(0, ex, ey, &aims[n].x, &aims[n].y);
            }
        CHECK(dgx_di_calibration_make(&c, &device, W, H, aims, n));
        CHECK(c.knots_x == 3 && c.knots_y == 0 && c.left_worst_tenths <= 10);
    }

    /* taken by a pointer of that device and display only, turned or not */
    {
        const int n = targets(aims, DGX_DI_SWAP_XY, skewed);
        CHECK(dgx_di_calibration_make(&c, &device, W, H, aims, n));
        dgx_di_pointer_t   p;
        const dgx_screen_t turned = {.width = H, .height = W, .dir_x = DgxScreenRightLeft, .dir_y = DgxScreenTopBottom, .swap_xy = true};
        const dgx_screen_t other  = {.width = 1024, .height = 600, .dir_x = DgxScreenLeftRight, .dir_y = DgxScreenTopBottom};
        dgx_di_pointer_init(&p, &device, 0, &turned);
        CHECK(dgx_di_pointer_calibrate(&p, &c) && p.flags == DGX_DI_SWAP_XY);
        int x, y;
        dgx_di_pointer_map(&p, aims[0].x, aims[0].y, &x, &y, NULL); /* meant at 48, 48 of its own: 48, 751 turned */
        CHECK(abs(x - 48) <= 1 && abs(y - (W - 1 - 48)) <= 1);
        dgx_di_pointer_init(&p, &device, 0, &other);
        CHECK(!dgx_di_pointer_calibrate(&p, &c));
    }

    /* how to steady it, from how it blinked */
    {
        dgx_di_calibration_steady(&c, 56, 90);
        CHECK(c.blink_ms == 56 && c.gap_ms == 90 && c.down_ms == 80 && c.up_ms == 110);
        dgx_di_calibration_steady(&c, 0, 0);
        CHECK(c.down_ms == 0 && c.up_ms == 30); /* up after 30 at least */
        dgx_di_calibration_steady(&c, 0, 20);
        CHECK(c.up_ms == 40);
        dgx_di_calibration_steady(&c, 0, 45);
        CHECK(c.up_ms == 70); /* 45 and 20, in tens up */
    }

    /* in words and as C: the whole length, cut to the room given */
    {
        const int n = targets(aims, DGX_DI_SWAP_XY | DGX_DI_MIRROR_Y, bent);
        CHECK(dgx_di_calibration_make(&c, &device, W, H, aims, n));
        c.rounds = 3;
        dgx_di_calibration_steady(&c, 56, 90);
        char      text[1000], cut[40];
        const int len = dgx_di_calibration_text(&c, text, sizeof text);
        CHECK(len == (int)strlen(text) && len < 400);
        CHECK(strstr(text, "800x1280") && strstr(text, "swapped, its y the other way") && strstr(text, "3 rounds") && strstr(text, "for up to 90 ms: down after 80 ms, up after 110"));
        dgx_di_calibration_steady(&c, 0, 132);
        dgx_di_calibration_text(&c, text, sizeof text);
        CHECK(strstr(text, "no finger before a press, and lost one held for up to 132 ms: up after 160 ms.") != NULL);
        dgx_di_calibration_steady(&c, 56, 0);
        dgx_di_calibration_text(&c, text, sizeof text);
        CHECK(strstr(text, "for up to 56 ms before a press, and never lost one held: down after 80 ms, up after 30.") != NULL);
        dgx_di_calibration_steady(&c, 0, 0);
        dgx_di_calibration_text(&c, text, sizeof text);
        CHECK(strstr(text, "neither showed a finger before a press nor lost one held: up after 30 ms.") != NULL);
        dgx_di_calibration_steady(&c, 56, 90);
        CHECK(dgx_di_calibration_text(&c, cut, sizeof cut) == len && strlen(cut) == sizeof cut - 1 && !strncmp(cut, text, sizeof cut - 1));
        CHECK(dgx_di_calibration_text(&c, NULL, 0) == len);
        const int code = dgx_di_calibration_code(&c, text, sizeof text);
        CHECK(code == (int)strlen(text));
        CHECK(strstr(text, ".flags = 6") && strstr(text, ".knots_x = 3") && strstr(text, ".x = {{") && strstr(text, ".y = {{") && strstr(text, ".size = sizeof(dgx_di_calibration_t)"));
        printf("%s\n", text);
        dgx_di_calibration_text(&c, text, sizeof text);
        printf("%s\n", text);
    }
    CHECK_DONE();
}
