/*
 * A calibration made from targets measured on a board, and what it says: plain C (the measuring itself, which draws and
 * waits, is dgx_di_calibrate.c).
 */
#include <math.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "dgx_di.h"

enum { AIMS_MAX = 25 }; /* a grid of 5 x 5 at most: the stack of a small chip's task holds it */
enum { UP_MS = 60 };    /* up after so long at least: the 10" lost a finger held for 132 ms in one measure, never in another */

/* the display in its own orientation, as a screen not turned */
static dgx_screen_t own(int width, int height)
{
    return (dgx_screen_t){.width = width, .height = height, .dir_x = DgxScreenLeftRight, .dir_y = DgxScreenTopBottom};
}

/* how far off from where it was meant: the larger of across and down */
static int off(int x, int y, int mx, int my)
{
    const int dx = abs(x - mx), dy = abs(y - my);
    return dx > dy ? dx : dy;
}

/* the places along an axis the targets were meant at, each where they are taken now on average; by taken in order */
static int knots(dgx_di_knot_t *k, const int16_t *meant, const int16_t *taken, int n)
{
    int32_t sum[DGX_DI_KNOTS];
    int     count[DGX_DI_KNOTS], places = 0;
    for (int i = 0; i < n; ++i) {
        int j = 0;
        while (j < places && k[j].meant != meant[i]) ++j;
        if (j == places) {
            if (places == DGX_DI_KNOTS) return 0; /* more places than points: none */
            k[places].meant = meant[i], sum[places] = 0, count[places] = 0, ++places;
        }
        sum[j] += taken[i], ++count[j];
    }
    if (places < 2) return 0;
    for (int j = 0; j < places; ++j) k[j].taken = (int16_t)((2 * sum[j] + (sum[j] >= 0 ? count[j] : -count[j])) / (2 * count[j]));
    for (int j = 1; j < places; ++j)
        for (int i = j; i > 0 && k[i].taken < k[i - 1].taken; --i) {
            const dgx_di_knot_t t = k[i];
            k[i] = k[i - 1], k[i - 1] = t;
        }
    /* one way along both: taken and meant go together, else the targets say nothing sure */
    for (int j = 1; j < places; ++j)
        if (k[j].taken <= k[j - 1].taken || k[j].meant <= k[j - 1].meant) return 0;
    return places;
}

/* how far off the targets are as the pointer takes them: tenths of a pixel on average, and the worst */
static void offs(const dgx_di_pointer_t *p, const dgx_di_aim_t *aims, int n, uint16_t *average, uint16_t *worst)
{
    int sum = 0, most = 0;
    for (int i = 0; i < n; ++i) {
        int x, y;
        dgx_di_pointer_map(p, aims[i].x, aims[i].y, &x, &y, NULL);
        const int o = off(x, y, aims[i].meant_x, aims[i].meant_y);
        sum += o, most = o > most ? o : most;
    }
    *average = (uint16_t)((20 * sum + n) / (2 * n)), *worst = (uint16_t)(10 * most);
}

bool dgx_di_calibration_make(dgx_di_calibration_t *c, const dgx_di_device_t *device, int width, int height, const dgx_di_aim_t *aims, int n)
{
    if (!c || !device || !aims || n < 4 || n > AIMS_MAX || width <= 0 || height <= 0) return false;
    /* which of the device's axes goes along which of the display's, and which way: how they go together over the targets */
    double rx = 0, ry = 0, mx = 0, my = 0;
    for (int i = 0; i < n; ++i) rx += aims[i].x, ry += aims[i].y, mx += aims[i].meant_x, my += aims[i].meant_y;
    rx /= n, ry /= n, mx /= n, my /= n;
    double xx = 0, xy = 0, yx = 0, yy = 0, vrx = 0, vry = 0, vmx = 0, vmy = 0;
    for (int i = 0; i < n; ++i) {
        const double a = aims[i].x - rx, b = aims[i].y - ry, u = aims[i].meant_x - mx, v = aims[i].meant_y - my;
        xx += a * u, xy += a * v, yx += b * u, yy += b * v, vrx += a * a, vry += b * b, vmx += u * u, vmy += v * v;
    }
    if (vrx <= 0 || vry <= 0 || vmx <= 0 || vmy <= 0) return false;
    xx /= sqrt(vrx * vmx), xy /= sqrt(vrx * vmy), yx /= sqrt(vry * vmx), yy /= sqrt(vry * vmy);
    int flags = 0;
    if (fabs(xy) + fabs(yx) > fabs(xx) + fabs(yy)) {
        /* its x along the display's y, its y along x */
        flags = DGX_DI_SWAP_XY | (yx < 0 ? DGX_DI_MIRROR_Y : 0) | (xy < 0 ? DGX_DI_MIRROR_X : 0);
    } else {
        flags = (xx < 0 ? DGX_DI_MIRROR_X : 0) | (yy < 0 ? DGX_DI_MIRROR_Y : 0);
    }
    memset(c, 0, sizeof *c);
    c->version = DGX_DI_CALIBRATION_VERSION, c->size = sizeof *c, c->flags = (uint8_t)flags;
    c->x_min = device->x_min, c->x_max = device->x_max, c->y_min = device->y_min, c->y_max = device->y_max;
    c->width = (int16_t)width, c->height = (int16_t)height;
    /* where the pointer takes each target by the flags alone */
    dgx_screen_t     screen = own(width, height);
    dgx_di_device_t  copy   = *device;
    dgx_di_pointer_t p;
    dgx_di_pointer_init(&p, &copy, flags, &screen);
    offs(&p, aims, n, &c->off_tenths, &c->worst_tenths);
    int16_t tx[AIMS_MAX], ty[AIMS_MAX];
    for (int i = 0; i < n; ++i) {
        int     x, y;
        int16_t at[2];
        dgx_di_pointer_map(&p, aims[i].x, aims[i].y, &x, &y, at);
        tx[i] = at[0], ty[i] = at[1];
    }
    /* the affine of least squares, about the middle of the targets as taken */
    double tmx = 0, tmy = 0;
    for (int i = 0; i < n; ++i) tmx += tx[i], tmy += ty[i];
    tmx /= n, tmy /= n;
    double uu = 0, uv = 0, vv = 0, ux = 0, vx = 0, uy = 0, vy = 0;
    for (int i = 0; i < n; ++i) {
        const double u = tx[i] - tmx, v = ty[i] - tmy, x = aims[i].meant_x - mx, y = aims[i].meant_y - my;
        uu += u * u, uv += u * v, vv += v * v, ux += u * x, vx += v * x, uy += u * y, vy += v * y;
    }
    const double det = uu * vv - uv * uv;
    if (!(det > 1e-6 * uu * vv)) return false; /* the targets in a line */
    const double a = (ux * vv - vx * uv) / det, b = (vx * uu - ux * uv) / det, d = (uy * vv - vy * uv) / det, e = (vy * uu - uy * uv) / det;
    const double f[6] = {a, b, mx - a * tmx - b * tmy, d, e, my - d * tmx - e * tmy};
    for (int i = 0; i < 6; ++i) c->affine[i] = (int32_t)lround(f[i] * 65536);
    /* then the points of each axis through what is left: the targets by where they were meant along it */
    dgx_di_pointer_calibrate(&p, c);
    int16_t ax[AIMS_MAX], ay[AIMS_MAX], meant_x[AIMS_MAX], meant_y[AIMS_MAX];
    for (int i = 0; i < n; ++i) {
        int x, y;
        dgx_di_pointer_map(&p, aims[i].x, aims[i].y, &x, &y, NULL);
        ax[i] = (int16_t)x, ay[i] = (int16_t)y, meant_x[i] = aims[i].meant_x, meant_y[i] = aims[i].meant_y;
    }
    c->knots_x = (uint8_t)knots(c->x, meant_x, ax, n), c->knots_y = (uint8_t)knots(c->y, meant_y, ay, n);
    if (!c->knots_x) memset(c->x, 0, sizeof c->x);
    if (!c->knots_y) memset(c->y, 0, sizeof c->y);
    offs(&p, aims, n, &c->left_tenths, &c->left_worst_tenths);
    return true;
}

void dgx_di_calibration_steady(dgx_di_calibration_t *c, int blink_ms, int gap_ms)
{
    blink_ms = blink_ms < 0 ? 0 : blink_ms > 60000 ? 60000 : blink_ms, gap_ms = gap_ms < 0 ? 0 : gap_ms > 60000 ? 60000 : gap_ms;
    c->blink_ms = (uint16_t)blink_ms, c->gap_ms = (uint16_t)gap_ms;
    /* past the longest by 20 ms, in tens; up never sooner than UP_MS: a measure may see no gap the panel has another time */
    c->down_ms = (uint16_t)(blink_ms ? (blink_ms + 29) / 10 * 10 : 0);
    c->up_ms   = (uint16_t)(gap_ms ? (gap_ms + 29) / 10 * 10 : 0);
    if (c->up_ms < UP_MS) c->up_ms = UP_MS;
}

/* text appended at *at, as snprintf counts: never past size, the whole length counted */
static void add(char *text, size_t size, int *at, const char *format, ...) __attribute__((format(printf, 4, 5)));

static void add(char *text, size_t size, int *at, const char *format, ...)
{
    if (!text) size = 0;
    const size_t from = (size_t)*at < size ? (size_t)*at : size;
    va_list      args;
    va_start(args, format);
    const int n = vsnprintf(size ? text + from : NULL, size - from, format, args);
    va_end(args);
    if (n > 0) *at += n;
}

int dgx_di_calibration_text(const dgx_di_calibration_t *c, char *text, size_t size)
{
    int at = 0;
    if (text && size) text[0] = 0;
    add(text, size, &at, "The panel lies under the display of %dx%d (its own orientation) ", c->width, c->height);
    if (!c->flags) add(text, size, &at, "as the display does");
    if (c->flags & DGX_DI_SWAP_XY) add(text, size, &at, "swapped%s", c->flags & (DGX_DI_MIRROR_X | DGX_DI_MIRROR_Y) ? ", " : "");
    if ((c->flags & DGX_DI_MIRROR_X) && (c->flags & DGX_DI_MIRROR_Y)) add(text, size, &at, "its x and y the other way");
    else if (c->flags & DGX_DI_MIRROR_X) add(text, size, &at, "its x the other way");
    else if (c->flags & DGX_DI_MIRROR_Y) add(text, size, &at, "its y the other way");
    add(text, size, &at, "; its touches were taken %d.%d pixels off on average and %d at worst without this calibration, %d.%d and %d with it (%d round%s). ",
        c->off_tenths / 10, c->off_tenths % 10, c->worst_tenths / 10, c->left_tenths / 10, c->left_tenths % 10, c->left_worst_tenths / 10,
        c->rounds, c->rounds == 1 ? "" : "s");
    if (c->blink_ms && c->gap_ms)
        add(text, size, &at, "Near the glass it showed a finger for up to %d ms before a press, and lost one held for up to %d ms: down after %d ms, up after %d.",
            c->blink_ms, c->gap_ms, c->down_ms, c->up_ms);
    else if (c->gap_ms)
        add(text, size, &at, "It showed no finger before a press, and lost one held for up to %d ms: up after %d ms.", c->gap_ms, c->up_ms);
    else if (c->blink_ms)
        add(text, size, &at, "Near the glass it showed a finger for up to %d ms before a press, and never lost one held: down after %d ms, up after %d.",
            c->blink_ms, c->down_ms, c->up_ms);
    else
        add(text, size, &at, "It neither showed a finger before a press nor lost one held: up after %d ms.", c->up_ms);
    return at;
}

int dgx_di_calibration_code(const dgx_di_calibration_t *c, char *text, size_t size)
{
    int at = 0;
    if (text && size) text[0] = 0;
    add(text, size, &at, "{.version = %d, .size = sizeof(dgx_di_calibration_t), .x_min = %d, .x_max = %d, .y_min = %d, .y_max = %d, .width = %d, .height = %d, ",
        c->version, c->x_min, c->x_max, c->y_min, c->y_max, c->width, c->height);
    add(text, size, &at, ".flags = %d, .knots_x = %d, .knots_y = %d, .rounds = %d, .affine = {%ld, %ld, %ld, %ld, %ld, %ld}", c->flags, c->knots_x, c->knots_y,
        c->rounds, (long)c->affine[0], (long)c->affine[1], (long)c->affine[2], (long)c->affine[3], (long)c->affine[4], (long)c->affine[5]);
    const struct {
        const char          *name;
        const dgx_di_knot_t *k;
        int                  n;
    } axes[2] = {{"x", c->x, c->knots_x}, {"y", c->y, c->knots_y}};
    for (int a = 0; a < 2; ++a) {
        if (!axes[a].n) continue;
        add(text, size, &at, ", .%s = {", axes[a].name);
        for (int i = 0; i < axes[a].n && i < DGX_DI_KNOTS; ++i) add(text, size, &at, "%s{%d, %d}", i ? ", " : "", axes[a].k[i].taken, axes[a].k[i].meant);
        add(text, size, &at, "}");
    }
    add(text, size, &at, ", .down_ms = %d, .up_ms = %d, .blink_ms = %d, .gap_ms = %d, .off_tenths = %d, .worst_tenths = %d, .left_tenths = %d, .left_worst_tenths = %d}",
        c->down_ms, c->up_ms, c->blink_ms, c->gap_ms, c->off_tenths, c->worst_tenths, c->left_tenths, c->left_worst_tenths);
    return at;
}
