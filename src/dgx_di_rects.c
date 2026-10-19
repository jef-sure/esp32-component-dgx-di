#include <string.h>

#include "dgx_di.h"

void dgx_di_rects_init(dgx_di_rects_t *r, dgx_di_rect_t *array, int count)
{
    memset(r, 0, sizeof *r);
    r->rects = array, r->count = array && count > 0 ? count : 0;
    for (int i = 0; i < DGX_DI_FINGERS; ++i) r->of[i] = -1;
    for (int k = 0; k < r->count; ++k) array[k].flags = 0, array[k].fingers = 0;
}

static bool inside(const dgx_di_rect_t *t, int x, int y)
{
    return x >= t->x && x < t->x + t->w && y >= t->y && y < t->y + t->h;
}

/* the first rectangle at x, y; -1: none */
static int hit(const dgx_di_rects_t *r, int x, int y)
{
    for (int k = 0; k < r->count; ++k)
        if (inside(&r->rects[k], x, y)) return k;
    return -1;
}

int dgx_di_rects_take(dgx_di_rects_t *r, const dgx_di_pointer_t *p, uint32_t now_ms)
{
    int first = -1;
    for (int k = 0; k < r->count; ++k) r->rects[k].flags = 0, r->rects[k].fingers = 0;
    for (int i = 0; i < DGX_DI_FINGERS; ++i) {
        const dgx_di_finger_t *f = &p->fingers[i];
        if (f->pressed) {
            /* down now: the rectangle it went down in is its own until it goes up; a finger claimed (a pad's) none */
            const int k = f->claimed ? -1 : hit(r, f->x, f->y);
            r->of[i] = (int8_t)k, r->strayed[i] = false, r->told[i] = false, r->since_ms[i] = now_ms;
            if (k >= 0) {
                r->rects[k].flags |= DGX_DI_RECT_PRESSED;
                if (first < 0 || k < first) first = k;
            }
        }
        const int k = r->of[i];
        if (k < 0 || k >= r->count) {
            r->of[i] = -1;
            continue;
        }
        dgx_di_rect_t *t  = &r->rects[k];
        const bool     in = inside(t, f->x, f->y);
        if (f->down) {
            t->flags |= DGX_DI_RECT_HELD | (in ? DGX_DI_RECT_INSIDE : 0), ++t->fingers;
            if (!in) r->strayed[i] = true;
            if (!r->told[i] && !r->strayed[i] && now_ms - r->since_ms[i] >= DGX_DI_LONG_MS) t->flags |= DGX_DI_RECT_LONG, r->told[i] = true;
        } else {
            /* up: let go inside and no long press before, a click */
            if (f->released) t->flags |= DGX_DI_RECT_RELEASED | (in && !r->told[i] ? DGX_DI_RECT_CLICKED : 0);
            r->of[i] = -1;
        }
    }
    return first;
}
