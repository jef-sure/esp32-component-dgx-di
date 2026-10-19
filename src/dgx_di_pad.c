#include "dgx_di_pad.h"

#include <math.h>
#include <stdlib.h>
#include <string.h>

#include "dgx_bits.h"
#include "dgx_draw.h"
#include "dgx_overlay.h"
#include "drivers/vscreen.h"

/* the colors of the layer: 0 nothing; a rim, a sign; then two of each item, its own and pressed */
enum { EMPTY = 0, RIM = 1, SIGN = 2, ITEM = 8, ITEMS_MAX = (256 - ITEM) / 2 };

const dgx_di_pad_item_t dgx_di_pad_classic[DGX_DI_PAD_CLASSIC] = {
    {DGX_DI_PAD_CROSS, DGX_DI_FROM_START, DGX_DI_FROM_END, 0, 300, 300, 200, 0, 0},
    {DGX_DI_PAD_BUTTON, DGX_DI_FROM_END, DGX_DI_FROM_END, 0, 300, 220, 70, DGX_DI_PAD_A, 0x2E65},  /* green */
    {DGX_DI_PAD_BUTTON, DGX_DI_FROM_END, DGX_DI_FROM_END, 0, 180, 340, 70, DGX_DI_PAD_B, 0xD8C3},  /* red */
    {DGX_DI_PAD_BUTTON, DGX_DI_FROM_END, DGX_DI_FROM_END, 0, 420, 340, 70, DGX_DI_PAD_X, 0x249D},  /* blue */
    {DGX_DI_PAD_BUTTON, DGX_DI_FROM_END, DGX_DI_FROM_END, 0, 300, 460, 70, DGX_DI_PAD_Y, 0xEE24},  /* yellow */
    {DGX_DI_PAD_BUTTON, DGX_DI_FROM_MIDDLE, DGX_DI_FROM_END, 0, -150, 120, 50, DGX_DI_PAD_BACK, 0},
    {DGX_DI_PAD_BUTTON, DGX_DI_FROM_MIDDLE, DGX_DI_FROM_END, 0, 150, 120, 50, DGX_DI_PAD_START, 0},
};

/* tenths of a millimetre in pixels along an axis of px pixels over mm millimetres */
static int pixels(int tenths, int px, int mm)
{
    return (int)(((int64_t)tenths * px * 2 + (tenths >= 0 ? 10 * mm : -10 * mm)) / (20 * (int64_t)mm));
}

static int place_of(int from, int at, int size)
{
    return from == DGX_DI_FROM_START ? at : from == DGX_DI_FROM_END ? size - 1 - at : size / 2 + at;
}

/* a pressed color: half way to white */
static uint16_t lighter(uint16_t c)
{
    const int r = (c >> 11) & 31, g = (c >> 5) & 63, b = c & 31;
    return (uint16_t)(((r + 31) / 2) << 11 | ((g + 63) / 2) << 5 | (b + 31) / 2);
}

/* half the width of a cross's arm: as it is drawn, as it is played */
static int arm(int r)
{
    return r * 7 / 20;
}

static uint32_t fill(int k, bool pressed)
{
    return (uint32_t)(ITEM + 2 * k + (pressed ? 1 : 0));
}

/* a stick's knob as packed in its state: so far from its middle each way, plus one (0: at rest) */
static uint32_t knob(int dx, int dy)
{
    return 1u + ((uint32_t)(dx + 2048) << 12 | (uint32_t)(dy + 2048));
}

static void draw(dgx_di_pad_t *pad, int k)
{
    const dgx_di_pad_item_t  *it = &pad->items[k];
    const dgx_di_pad_place_t *pl = &pad->places[k];
    dgx_screen_t             *l  = pad->layer;
    const int                 x = pl->x, y = pl->y, r = pl->r, s = pl->state;
    dgx_screen_progress_up(l);
    switch (it->kind) {
    case DGX_DI_PAD_BUTTON: {
        dgx_solid_circle(l, x, y, r, fill(k, s != 0));
        dgx_draw_circle(l, x, y, r, RIM), dgx_draw_circle(l, x, y, r - 1, RIM);
        const int q = r / 3;
        if (it->out == DGX_DI_PAD_START) dgx_draw_triangle_solid(l, x - q, y - q, x - q, y + q, x + q, y, SIGN);
        if (it->out == DGX_DI_PAD_BACK) dgx_draw_triangle_solid(l, x + q, y - q, x + q, y + q, x - q, y, SIGN);
        if (it->out == DGX_DI_PAD_HOME) dgx_fill_rectangle(l, x - q, y - q, 2 * q + 1, 2 * q + 1, SIGN);
        break;
    }
    case DGX_DI_PAD_CROSS: {
        /* its arms, those held lighter, an arrow at each end */
        const int a = arm(r), e = r / 5;
        dgx_solid_circle(l, x, y, r, EMPTY);
        dgx_fill_rectangle(l, x - r, y - a, 2 * r + 1, 2 * a + 1, fill(k, false));
        dgx_fill_rectangle(l, x - a, y - r, 2 * a + 1, 2 * r + 1, fill(k, false));
        if (s & DGX_DI_PAD_LEFT) dgx_fill_rectangle(l, x - r, y - a, r - a, 2 * a + 1, fill(k, true));
        if (s & DGX_DI_PAD_RIGHT) dgx_fill_rectangle(l, x + a + 1, y - a, r - a, 2 * a + 1, fill(k, true));
        if (s & DGX_DI_PAD_UP) dgx_fill_rectangle(l, x - a, y - r, 2 * a + 1, r - a, fill(k, true));
        if (s & DGX_DI_PAD_DOWN) dgx_fill_rectangle(l, x - a, y + a + 1, 2 * a + 1, r - a, fill(k, true));
        dgx_draw_triangle_solid(l, x - r + e, y, x - r + 2 * e, y - e, x - r + 2 * e, y + e, SIGN);
        dgx_draw_triangle_solid(l, x + r - e, y, x + r - 2 * e, y - e, x + r - 2 * e, y + e, SIGN);
        dgx_draw_triangle_solid(l, x, y - r + e, x - e, y - r + 2 * e, x + e, y - r + 2 * e, SIGN);
        dgx_draw_triangle_solid(l, x, y + r - e, x - e, y + r - 2 * e, x + e, y + r - 2 * e, SIGN);
        break;
    }
    case DGX_DI_PAD_STICK: {
        /* its well, its knob where the finger has it, half the way (it stays inside the well) */
        const int dx = s ? (int)((s - 1) >> 12) - 2048 : 0, dy = s ? (int)((s - 1) & 4095) - 2048 : 0;
        dgx_solid_circle(l, x, y, r, fill(k, false));
        dgx_draw_circle(l, x, y, r, RIM);
        dgx_solid_circle(l, x + dx, y + dy, r / 2, fill(k, s != 0));
        dgx_draw_circle(l, x + dx, y + dy, r / 2, RIM);
        break;
    }
    }
    dgx_screen_progress_down(l);
    pad->places[k].drawn = pl->state;
}

/* the mask: each item's circle, the first item where two meet */
static void mark(dgx_di_pad_t *pad, int k)
{
    const dgx_di_pad_place_t *pl = &pad->places[k];
    const int                 r  = pl->r;
    for (int y = pl->y - r; y <= pl->y + r; ++y) {
        if (y < 0 || y >= pad->height) continue;
        for (int x = pl->x - r; x <= pl->x + r; ++x) {
            const int dx = x - pl->x, dy = y - pl->y;
            if (x < 0 || x >= pad->width || dx * dx + dy * dy > r * r) continue;
            uint8_t *m = &pad->mask[(size_t)y * pad->width + x];
            if (!*m) *m = (uint8_t)(k + 1);
        }
    }
}

bool dgx_di_pad_init(dgx_di_pad_t *pad, const dgx_screen_t *display, int width_mm, int height_mm, const dgx_di_pad_item_t *items, int count)
{
    memset(pad, 0, sizeof *pad);
    for (int i = 0; i < DGX_DI_FINGERS; ++i) pad->of[i] = -1;
    if (!display || width_mm <= 0 || height_mm <= 0 || !items || count <= 0 || count > ITEMS_MAX) return false;
    const int w = display->width, h = display->height;
    pad->items = items, pad->count = count, pad->width = (int16_t)w, pad->height = (int16_t)h;
    pad->places = calloc((size_t)count, sizeof *pad->places);
    pad->mask   = calloc((size_t)w * h, 1);
    pad->layer  = dgx_vscreen_init(w, h, 8, DgxScreenRGB);
    if (!pad->places || !pad->mask || !pad->layer) {
        dgx_di_pad_free(pad, NULL);
        return false;
    }
    for (int k = 0; k < count; ++k) {
        const dgx_di_pad_item_t *it = &items[k];
        /* a radius by both ways of the display, so that it stays round on pixels not square */
        const int r = (pixels(it->r, w, width_mm) + pixels(it->r, h, height_mm) + 1) / 2;
        pad->places[k] = (dgx_di_pad_place_t){.x = (int16_t)place_of(it->from_x, pixels(it->x, w, width_mm), w),
                                              .y = (int16_t)place_of(it->from_y, pixels(it->y, h, height_mm), h), .r = (int16_t)(r < 2 ? 2 : r)};
        mark(pad, k);
        draw(pad, k);
    }
    return true;
}

void dgx_di_pad_free(dgx_di_pad_t *pad, dgx_screen_t *display)
{
    if (display && pad->layer) dgx_di_pad_hide(pad, display);
    if (pad->layer) dgx_screen_destroy(&pad->layer);
    free(pad->mask), free(pad->places);
    pad->mask = NULL, pad->places = NULL, pad->count = 0;
}

bool dgx_di_pad_show(dgx_di_pad_t *pad, dgx_screen_t *display, uint8_t alpha)
{
    uint16_t lut[256] = {0};
    lut[RIM] = dgx_rgb_to_16(225, 225, 225), lut[SIGN] = dgx_rgb_to_16(255, 255, 255);
    for (int k = 0; k < pad->count; ++k) {
        const uint16_t c = pad->items[k].color ? pad->items[k].color : dgx_rgb_to_16(90, 90, 90);
        lut[fill(k, false)] = c, lut[fill(k, true)] = lighter(c);
    }
    return pad->layer && dgx_overlay_set(display, pad->layer, 0, 0, lut, alpha);
}

void dgx_di_pad_hide(dgx_di_pad_t *pad, dgx_screen_t *display)
{
    (void)pad;
    dgx_overlay_clear(display);
}

int dgx_di_pad_item_at(const dgx_di_pad_t *pad, int x, int y)
{
    if (!pad->mask || x < 0 || y < 0 || x >= pad->width || y >= pad->height) return -1;
    return pad->mask[(size_t)y * pad->width + x] - 1;
}

/*
 * a cross's arrows for a finger dx, dy from its middle, as the cross is drawn: none at its middle; on an arm, that
 * arm's way; in a corner between two arms, both (a cross of four ways: the arm it is nearer)
 */
static uint32_t ways(int dx, int dy, int r, bool four)
{
    const int64_t d2 = (int64_t)dx * dx + (int64_t)dy * dy;
    if (d2 * 400 < (int64_t)r * r * 9) return 0; /* within 15 % of its radius */
    const uint32_t across = dx > 0 ? DGX_DI_PAD_RIGHT : DGX_DI_PAD_LEFT, along = dy > 0 ? DGX_DI_PAD_DOWN : DGX_DI_PAD_UP;
    const int      ax = abs(dx), ay = abs(dy), a = arm(r);
    if (four || ax <= a || ay <= a) return ax >= ay ? across : along;
    return across | along;
}

static int16_t axis(int d, int r)
{
    const int v = (int)lround((double)d * DGX_DI_AXIS_MAX / r);
    return (int16_t)(v > DGX_DI_AXIS_MAX ? DGX_DI_AXIS_MAX : v < -DGX_DI_AXIS_MAX ? -DGX_DI_AXIS_MAX : v);
}

void dgx_di_pad_take(dgx_di_pad_t *pad, dgx_di_pointer_t *p)
{
    uint32_t buttons = 0;
    memset(pad->axes, 0, sizeof pad->axes);
    for (int k = 0; k < pad->count; ++k) pad->places[k].state = 0;
    for (int i = 0; i < DGX_DI_FINGERS; ++i) {
        const dgx_di_finger_t *f = &p->fingers[i];
        if (f->pressed) {
            /* down on an item: the pad's till it goes up */
            const int k = dgx_di_pad_item_at(pad, f->x, f->y);
            pad->of[i] = (int8_t)k;
            if (k >= 0) dgx_di_pointer_claim(p, i), pad->down_x[i] = f->x, pad->down_y[i] = f->y;
        }
        const int k = pad->of[i];
        if (!f->down || k < 0 || k >= pad->count) {
            pad->of[i] = -1;
            continue;
        }
        const dgx_di_pad_item_t *it = &pad->items[k];
        dgx_di_pad_place_t      *pl = &pad->places[k];
        int                      dx = f->x - pl->x, dy = f->y - pl->y;
        switch (it->kind) {
        case DGX_DI_PAD_BUTTON:
            /* held within half its radius again from its middle */
            if (4 * ((int64_t)dx * dx + (int64_t)dy * dy) <= 9 * (int64_t)pl->r * pl->r) buttons |= it->out, pl->state = 1;
            break;
        case DGX_DI_PAD_CROSS: {
            const uint32_t w = ways(dx, dy, pl->r, it->ways & DGX_DI_PAD_FOUR);
            buttons |= w, pl->state |= w;
            break;
        }
        case DGX_DI_PAD_STICK: {
            if (it->ways & DGX_DI_PAD_FLOATING) dx = f->x - pad->down_x[i], dy = f->y - pad->down_y[i];
            /* within its radius as it is, past it at its rim the same way */
            const double d = sqrt((double)dx * dx + (double)dy * dy);
            if (d > pl->r) dx = (int)lround(dx * pl->r / d), dy = (int)lround(dy * pl->r / d);
            const int a = it->out ? DGX_DI_AXIS_RX : DGX_DI_AXIS_LX;
            pad->axes[a] = axis(dx, pl->r), pad->axes[a + 1] = axis(dy, pl->r), pl->state = knob(dx / 2, dy / 2);
            break;
        }
        }
    }
    pad->pressed = buttons & ~pad->buttons, pad->released = pad->buttons & ~buttons, pad->buttons = buttons;
    for (int k = 0; k < pad->count; ++k)
        if (pad->places[k].state != pad->places[k].drawn) draw(pad, k);
}
