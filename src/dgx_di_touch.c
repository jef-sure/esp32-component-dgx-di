#include <stdlib.h>

#include "dgx_di.h"

void dgx_di_touch_display(dgx_di_touch_t *t, const dgx_screen_t *screen, int width_mm, int height_mm)
{
    const bool known = screen && width_mm > 0 && height_mm > 0;
    /* kept in the display's own orientation, so that they turn with it */
    t->screen = known ? screen : NULL;
    t->mm_w   = (int16_t)(!known ? 0 : screen->swap_xy ? height_mm : width_mm);
    t->mm_h   = (int16_t)(!known ? 0 : screen->swap_xy ? width_mm : height_mm);
}

/* so many millimetres in pixels of an axis of px pixels over of_mm millimetres, rounded */
static int pixels(int mm, int px, int of_mm)
{
    return (mm * px + of_mm / 2) / of_mm;
}

void dgx_di_touch_take(dgx_di_touch_t *t, int x, int y, bool down, uint32_t now_ms)
{
    const dgx_screen_t *s  = t->screen;
    const int           w  = s ? s->width : 0, h = s ? s->height : 0;
    const int           mw = s ? (s->swap_xy ? t->mm_h : t->mm_w) : 0, mh = s ? (s->swap_xy ? t->mm_w : t->mm_h) : 0;
    t->pressed  = down && !t->down;
    t->released = !down && t->down;
    t->down     = down;
    t->x = (int16_t)x, t->y = (int16_t)y;
    t->edge = 0, t->swipe = 0, t->long_press = false;
    if (t->pressed) {
        /* down near an edge: it may become a swipe from it */
        t->from = 0, t->told = false, t->strayed = false, t->from_x = (int16_t)x, t->from_y = (int16_t)y, t->from_ms = now_ms;
        if (mw > 0 && mh > 0) {
            const int bx = pixels(DGX_DI_EDGE_BAND_MM, w, mw), by = pixels(DGX_DI_EDGE_BAND_MM, h, mh);
            if (y < by) t->from |= DGX_DI_EDGE_TOP;
            if (y >= h - by) t->from |= DGX_DI_EDGE_BOTTOM;
            if (x < bx) t->from |= DGX_DI_EDGE_LEFT;
            if (x >= w - bx) t->from |= DGX_DI_EDGE_RIGHT;
        }
    }
    if (!down) {
        t->from = 0;
        return;
    }
    if (t->told || mw <= 0 || mh <= 0) return;
    /* away from the edge far enough: the swipe from it */
    const int dx = x - t->from_x, dy = y - t->from_y;
    const int tx = pixels(DGX_DI_SWIPE_TRAVEL_MM, w, mw), ty = pixels(DGX_DI_SWIPE_TRAVEL_MM, h, mh);
    if ((t->from & DGX_DI_EDGE_TOP) && dy >= ty) t->edge = DGX_DI_EDGE_TOP;
    else if ((t->from & DGX_DI_EDGE_BOTTOM) && -dy >= ty) t->edge = DGX_DI_EDGE_BOTTOM;
    else if ((t->from & DGX_DI_EDGE_LEFT) && dx >= tx) t->edge = DGX_DI_EDGE_LEFT;
    else if ((t->from & DGX_DI_EDGE_RIGHT) && -dx >= tx) t->edge = DGX_DI_EDGE_RIGHT;
    else {
        /* else far enough along an axis: the way it went; along both, the one gone the larger share of its way */
        const bool along_x = abs(dx) >= tx, along_y = abs(dy) >= ty;
        if (along_x && (!along_y || abs(dx) * ty > abs(dy) * tx)) t->swipe = dx > 0 ? DGX_DI_SWIPE_RIGHT : DGX_DI_SWIPE_LEFT;
        else if (along_y) t->swipe = dy > 0 ? DGX_DI_SWIPE_DOWN : DGX_DI_SWIPE_UP;
    }
    /* else held long where it went down: a long press — but not while it may still be a swipe from an edge (a finger
       rests at the edge before it sets out) */
    if (abs(dx) > pixels(DGX_DI_LONG_SLOP_MM, w, mw) || abs(dy) > pixels(DGX_DI_LONG_SLOP_MM, h, mh)) t->strayed = true;
    if (!t->edge && !t->swipe && !t->strayed && !t->from && now_ms - t->from_ms >= DGX_DI_LONG_MS) t->long_press = true;
    if (t->edge || t->swipe || t->long_press) t->from = 0, t->told = true;
}
