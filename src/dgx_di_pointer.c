#include <string.h>

#include "dgx_di.h"

void dgx_di_pointer_init(dgx_di_pointer_t *p, dgx_di_device_t *device, int flags, const dgx_screen_t *screen)
{
    memset(p, 0, sizeof *p);
    p->device = device, p->screen = screen, p->flags = (uint8_t)flags, p->first = -1;
}

void dgx_di_pointer_debounce(dgx_di_pointer_t *p, int down_ms, int up_ms)
{
    p->down_ms = (uint16_t)(down_ms < 0 ? 0 : down_ms > 60000 ? 60000 : down_ms);
    p->up_ms   = (uint16_t)(up_ms < 0 ? 0 : up_ms > 60000 ? 60000 : up_ms);
}

bool dgx_di_pointer_calibrate(dgx_di_pointer_t *p, const dgx_di_calibration_t *c)
{
    if (!c) {
        p->calibration = NULL;
        return true;
    }
    const dgx_di_device_t *d = p->device;
    const dgx_screen_t    *s = p->screen;
    if (!d || !s || c->version != DGX_DI_CALIBRATION_VERSION || c->size != sizeof *c || c->knots_x > DGX_DI_KNOTS || c->knots_y > DGX_DI_KNOTS)
        return false;
    /* for this device and this display */
    const int dw = s->swap_xy ? s->height : s->width, dh = s->swap_xy ? s->width : s->height;
    if (c->x_min != d->x_min || c->x_max != d->x_max || c->y_min != d->y_min || c->y_max != d->y_max || c->width != dw || c->height != dh) return false;
    p->calibration = c, p->flags = c->flags;
    if (c->down_ms || c->up_ms) dgx_di_pointer_debounce(p, c->down_ms, c->up_ms);
    return true;
}

static int within(int v, int n)
{
    return v < 0 ? 0 : v >= n ? n - 1 : v;
}

/* where meant by the points: the piece v falls in (the end's past the ends), in proportion, rounded */
static int corrected(int v, const dgx_di_knot_t *k, int n, int size)
{
    if (n < 2) return n ? within(v + k[0].meant - k[0].taken, size) : v;
    int i = 0;
    while (i < n - 2 && v > k[i + 1].taken) ++i;
    const int32_t span = k[i + 1].taken - k[i].taken, num = (int32_t)(v - k[i].taken) * (k[i + 1].meant - k[i].meant);
    if (span <= 0) return within(v + k[i].meant - k[i].taken, size);
    return within(k[i].meant + (int)((2 * num + (num >= 0 ? span : -span)) / (2 * span)), size);
}

void dgx_di_pointer_map(const dgx_di_pointer_t *p, int dx, int dy, int *x, int *y, int16_t taken[2])
{
    const dgx_di_device_t *d = p->device;
    const dgx_screen_t    *s = p->screen;
    const int              rx = d->x_max - d->x_min + 1, ry = d->y_max - d->y_min + 1;
    int                    w = rx < 1 ? 1 : rx, h = ry < 1 ? 1 : ry;
    dx = within(dx - d->x_min, w), dy = within(dy - d->y_min, h);
    if (p->flags & DGX_DI_MIRROR_X) dx = w - 1 - dx;
    if (p->flags & DGX_DI_MIRROR_Y) dy = h - 1 - dy;
    if (p->flags & DGX_DI_SWAP_XY) {
        const int t = dx, u = w;
        dx = dy, dy = t, w = h, h = u;
    }
    /* the display in its own orientation; a cell of the device to the display's pixel under its middle */
    const int dw = s->swap_xy ? s->height : s->width, dh = s->swap_xy ? s->width : s->height;
    int       sx = (int)(((2 * dx + 1) * (int32_t)dw) / (2 * w)), sy = (int)(((2 * dy + 1) * (int32_t)dh) / (2 * h));
    if (taken) taken[0] = (int16_t)sx, taken[1] = (int16_t)sy;
    const dgx_di_calibration_t *c = p->calibration;
    if (c) {
        /* the affine (16 bits of fraction), then the points of each axis */
        const int32_t *a = c->affine;
        if (a[0] || a[1] || a[2] || a[3] || a[4] || a[5]) {
            const int64_t vx = ((int64_t)a[0] * sx + (int64_t)a[1] * sy + a[2] + 32768) >> 16;
            const int64_t vy = ((int64_t)a[3] * sx + (int64_t)a[4] * sy + a[5] + 32768) >> 16;
            sx = vx < 0 ? 0 : vx >= dw ? dw - 1 : (int)vx, sy = vy < 0 ? 0 : vy >= dh ? dh - 1 : (int)vy;
        }
        sx = corrected(sx, c->x, c->knots_x, dw), sy = corrected(sy, c->y, c->knots_y, dh);
    }
    /* as the screen is turned now (bus/dgx_fb_area_priv.h of DGX): its axes swapped, then counted the other way along the panel's */
    if (s->dir_x == DgxScreenRightLeft) sx = dw - 1 - sx;
    if (s->dir_y == DgxScreenBottomTop) sy = dh - 1 - sy;
    *x = s->swap_xy ? sy : sx, *y = s->swap_xy ? sx : sy;
}

static void first_of_free(dgx_di_pointer_t *p);

int dgx_di_pointer_read(dgx_di_pointer_t *p, uint32_t now_ms)
{
    dgx_di_point_t pts[DGX_DI_FINGERS];
    int16_t        at[DGX_DI_FINGERS][4]; /* on the display, then before the calibration and the turn */
    int8_t         of[DGX_DI_FINGERS];    /* the finger each point is; -1: none yet */
    bool           taken[DGX_DI_FINGERS] = {false}, was[DGX_DI_FINGERS];
    const int      max = !p->device ? 0 : !p->device->fingers ? 1 : p->device->fingers < DGX_DI_FINGERS ? p->device->fingers : DGX_DI_FINGERS;
    int            n   = max && p->device->read && p->screen ? p->device->read(p->device, pts, max) : 0;
    if (n < 0) n = 0;
    if (n > max) n = max;
    for (int i = 0; i < DGX_DI_FINGERS; ++i) {
        dgx_di_finger_t *f = &p->fingers[i];
        was[i] = f->sees != f->down; /* running against down before this read; released before: free now */
        f->pressed = f->released = false;
    }
    for (int k = 0; k < n; ++k) {
        int x, y;
        dgx_di_pointer_map(p, pts[k].x, pts[k].y, &x, &y, &at[k][2]);
        at[k][0] = (int16_t)x, at[k][1] = (int16_t)y, of[k] = -1;
        /* known by the device's number */
        for (int i = 0; pts[k].id != DGX_DI_NO_ID && i < DGX_DI_FINGERS; ++i) {
            const dgx_di_finger_t *f = &p->fingers[i];
            if ((f->down || f->sees) && !taken[i] && f->id == pts[k].id) {
                of[k] = (int8_t)i, taken[i] = true;
                break;
            }
        }
    }
    /* with no number: the nearest point and finger of the last read first, and so on */
    for (;;) {
        int32_t best = INT32_MAX;
        int     bk = -1, bi = -1;
        for (int k = 0; k < n; ++k) {
            if (pts[k].id != DGX_DI_NO_ID || of[k] >= 0) continue;
            for (int i = 0; i < DGX_DI_FINGERS; ++i) {
                const dgx_di_finger_t *f = &p->fingers[i];
                if (!(f->down || f->sees) || taken[i]) continue;
                const int32_t ex = f->seen_x - at[k][0], ey = f->seen_y - at[k][1], e = ex * ex + ey * ey;
                if (e < best) best = e, bk = k, bi = i;
            }
        }
        if (bk < 0) break;
        of[bk] = (int8_t)bi, taken[bi] = true;
    }
    /* the rest: new fingers, in places none uses */
    for (int k = 0; k < n; ++k) {
        for (int i = 0; of[k] < 0 && i < DGX_DI_FINGERS; ++i) {
            dgx_di_finger_t *f = &p->fingers[i];
            if (f->down || f->sees || taken[i]) continue;
            memset(f, 0, sizeof *f);
            of[k] = (int8_t)i, taken[i] = true, was[i] = false;
            if (pts[k].id != DGX_DI_NO_ID) {
                f->id = pts[k].id;
            } else {
                f->id      = p->next_id;
                p->next_id = (uint8_t)(p->next_id + 1 == DGX_DI_NO_ID ? 0 : p->next_id + 1);
            }
        }
        if (of[k] < 0) continue;
        dgx_di_finger_t *f = &p->fingers[of[k]];
        f->sees = true, f->seen_x = at[k][0], f->seen_y = at[k][1], f->taken_x = at[k][2], f->taken_y = at[k][3];
        f->size = pts[k].size;
    }
    int down = 0;
    for (int i = 0; i < DGX_DI_FINGERS; ++i) {
        dgx_di_finger_t *f = &p->fingers[i];
        if (!taken[i]) f->sees = false;
        /* steadied: the other way only after so long running */
        if (f->sees != f->down) {
            if (!was[i]) {
                f->since_ms = now_ms;
                if (f->sees) f->landed_x = f->seen_x, f->landed_y = f->seen_y;
            }
            if (now_ms - f->since_ms >= (f->down ? p->up_ms : p->down_ms)) f->down = f->sees, f->pressed = f->down, f->released = !f->down;
        }
        /* down: where it is; gone down now, where it was first seen: the steadying took that long */
        if (f->down && f->sees) f->x = f->pressed ? f->landed_x : f->seen_x, f->y = f->pressed ? f->landed_y : f->seen_y;
        down += f->down;
    }
    /* the first finger: down when none was, until up; after it none until all are up */
    if (p->first >= 0) {
        const dgx_di_finger_t *f = &p->fingers[p->first];
        p->first_x = f->x, p->first_y = f->y;
        if (!f->down) p->first = -1, p->first_up = true;
    }
    first_of_free(p);
    return down;
}

/* none first while a free finger stays down after the first went up; else a free finger gone down now */
static void first_of_free(dgx_di_pointer_t *p)
{
    bool free_down = false;
    for (int i = 0; i < DGX_DI_FINGERS; ++i) free_down |= p->fingers[i].down && !p->fingers[i].claimed;
    if (!free_down) p->first_up = false;
    for (int i = 0; p->first < 0 && !p->first_up && i < DGX_DI_FINGERS; ++i)
        if (p->fingers[i].pressed && !p->fingers[i].claimed) p->first = (int8_t)i, p->first_x = p->fingers[i].x, p->first_y = p->fingers[i].y;
}

void dgx_di_pointer_claim(dgx_di_pointer_t *p, int i)
{
    if (i < 0 || i >= DGX_DI_FINGERS) return;
    p->fingers[i].claimed = true;
    /* it was taken for the first finger at this read: another one, if another went down at it */
    if (p->first == i) p->first = -1;
    first_of_free(p);
}

bool dgx_di_pointer_first(const dgx_di_pointer_t *p, int *x, int *y)
{
    *x = p->first_x, *y = p->first_y;
    return p->first >= 0;
}
