#include "dgx_di_pad.h"

#include <math.h>
#include <stdlib.h>
#include <string.h>

#include "dgx_bits.h"
#include "dgx_draw.h"
#include "dgx_overlay.h"
#include "drivers/vscreen.h"

/*
 * The colors of the layer: 0 nothing; the rest as they are used, the rim and the signs of the default look first, then
 * each item's own and pressed, then the colors of its pictures (pad->palette).
 */
enum { EMPTY = 0, ITEMS_MAX = 124 };

/* The blob of tools/pad_blob.py (its format is told there): little-endian, read where it lies, byte by byte. */
enum { HEADER = 16, ITEM_BYTES = 56, SHAPE_BYTES = 20, POINT_BYTES = 4, NONE = 0xffff, PICTURES = 9 };
enum { ITEM_PICTURES = 16, ITEM_PICTURED = 52 }; /* in an item: its pictures (first shape, how many), whether it is drawn by them */
enum { CIRCLE, RING, RECT, POLYGON, LINE };
enum { OWN, ITEM_COLOR, LIGHT, DARK };
/*
 * the pictures of each kind: a button's normal and pressed; a cross's normal, then an arm held of each way, then a corner
 * held of each diagonal (both its ways); a stick's well, knob, knob held
 */
static const uint8_t pictures_of[3] = {2, 9, 3};
enum { ARM_UP = 1, ARM_DOWN, ARM_LEFT, ARM_RIGHT, UP_LEFT, UP_RIGHT, DOWN_LEFT, DOWN_RIGHT };
enum { WELL = 0, KNOB, KNOB_HELD };

const dgx_di_pad_item_t dgx_di_pad_classic[DGX_DI_PAD_CLASSIC] = {
    {DGX_DI_PAD_CROSS, DGX_DI_FROM_START, DGX_DI_FROM_END, 0, 300, 300, 200, 0, 0},
    {DGX_DI_PAD_BUTTON, DGX_DI_FROM_END, DGX_DI_FROM_END, 0, 300, 220, 70, DGX_DI_PAD_A, 0x2E65},  /* green */
    {DGX_DI_PAD_BUTTON, DGX_DI_FROM_END, DGX_DI_FROM_END, 0, 180, 340, 70, DGX_DI_PAD_B, 0xD8C3},  /* red */
    {DGX_DI_PAD_BUTTON, DGX_DI_FROM_END, DGX_DI_FROM_END, 0, 420, 340, 70, DGX_DI_PAD_X, 0x249D},  /* blue */
    {DGX_DI_PAD_BUTTON, DGX_DI_FROM_END, DGX_DI_FROM_END, 0, 300, 460, 70, DGX_DI_PAD_Y, 0xEE24},  /* yellow */
    {DGX_DI_PAD_BUTTON, DGX_DI_FROM_MIDDLE, DGX_DI_FROM_END, 0, -150, 120, 50, DGX_DI_PAD_BACK, 0},
    {DGX_DI_PAD_BUTTON, DGX_DI_FROM_MIDDLE, DGX_DI_FROM_END, 0, 150, 120, 50, DGX_DI_PAD_START, 0},
};

static uint16_t u16(const uint8_t *p)
{
    return (uint16_t)(p[0] | p[1] << 8);
}

static int16_t i16(const uint8_t *p)
{
    return (int16_t)u16(p);
}

static uint32_t u32(const uint8_t *p)
{
    return (uint32_t)p[0] | (uint32_t)p[1] << 8 | (uint32_t)p[2] << 16 | (uint32_t)p[3] << 24;
}

/* tenths of a millimetre in pixels along an axis of px pixels over mm millimetres */
static int pixels(int tenths, int px, int mm)
{
    return (int)(((int64_t)tenths * px * 2 + (tenths >= 0 ? 10 * mm : -10 * mm)) / (20 * (int64_t)mm));
}

/* along x, along y, and a length of both (a radius, a width: round on pixels not square) */
static int px_x(const dgx_di_pad_t *pad, int t)
{
    return pixels(t, pad->width, pad->mm_w);
}

static int px_y(const dgx_di_pad_t *pad, int t)
{
    return pixels(t, pad->height, pad->mm_h);
}

static int px_r(const dgx_di_pad_t *pad, int t)
{
    return (px_x(pad, t) + px_y(pad, t) + 1) / 2;
}

static int place_of(int from, int at, int size)
{
    return from == DGX_DI_FROM_START ? at : from == DGX_DI_FROM_END ? size - 1 - at : size / 2 + at;
}

/* a pressed color: half way to white; a darker one: half way to black */
static uint16_t lighter(uint16_t c)
{
    const int r = (c >> 11) & 31, g = (c >> 5) & 63, b = c & 31;
    return (uint16_t)(((r + 31) / 2) << 11 | ((g + 63) / 2) << 5 | (b + 31) / 2);
}

static uint16_t darker(uint16_t c)
{
    const int r = (c >> 11) & 31, g = (c >> 5) & 63, b = c & 31;
    return (uint16_t)((r / 2) << 11 | (g / 2) << 5 | b / 2);
}

/* a quarter of the way to white: a cross's brackets at rest */
static uint16_t softer(uint16_t c)
{
    const int r = (c >> 11) & 31, g = (c >> 5) & 63, b = c & 31;
    return (uint16_t)(((3 * r + 31) / 4) << 11 | ((3 * g + 63) / 4) << 5 | (3 * b + 31) / 4);
}

static uint16_t color_of(const dgx_di_pad_t *pad, int k)
{
    return pad->items[k].color ? pad->items[k].color : (uint16_t)dgx_rgb_to_16(90, 90, 90);
}

/* the index of a color in the layer's table: found, else added, else (all 255 taken) the nearest */
static int paint(dgx_di_pad_t *pad, uint16_t c)
{
    for (int i = 1; i < pad->colors; ++i)
        if (pad->palette[i] == c) return i;
    if (pad->colors < 256) {
        pad->palette[pad->colors] = c;
        return pad->colors++;
    }
    int best = 1, most = INT32_MAX;
    for (int i = 1; i < 256; ++i) {
        const int dr = (pad->palette[i] >> 11) - (c >> 11), dg = ((pad->palette[i] >> 5) & 63) - ((c >> 5) & 63), db = (pad->palette[i] & 31) - (c & 31);
        const int d  = 4 * dr * dr + dg * dg + 4 * db * db;
        if (d < most) most = d, best = i;
    }
    return best;
}

static uint16_t rim_color(void)
{
    return (uint16_t)dgx_rgb_to_16(225, 225, 225);
}

static uint16_t sign_color(void)
{
    return (uint16_t)dgx_rgb_to_16(255, 255, 255);
}

/* half the width of a cross's arm: as it is drawn by default, as it is played */
static int arm(int r)
{
    return r * 7 / 20;
}

/* a stick's knob as packed in its state: so far from its middle each way, plus one (0: at rest) */
static uint32_t knob(int dx, int dy)
{
    return 1u + ((uint32_t)(dx + 2048) << 12 | (uint32_t)(dy + 2048));
}

static void knob_of(uint32_t s, int *dx, int *dy)
{
    *dx = s ? (int)((s - 1) >> 12) - 2048 : 0, *dy = s ? (int)((s - 1) & 4095) - 2048 : 0;
}

/* --- the blob */

static const uint8_t *item_bytes(const dgx_di_pad_t *pad, int k)
{
    return pad->blob + HEADER + ITEM_BYTES * k;
}

static const uint8_t *shape_bytes(const dgx_di_pad_t *pad, int s)
{
    return pad->blob + HEADER + ITEM_BYTES * pad->count + SHAPE_BYTES * s;
}

static const uint8_t *point_bytes(const dgx_di_pad_t *pad, int i)
{
    return pad->blob + HEADER + ITEM_BYTES * pad->count + SHAPE_BYTES * u16(pad->blob + 6) + POINT_BYTES * i;
}

/* the shapes of a picture of an item: the first, how many */
static int picture_of(const dgx_di_pad_t *pad, int k, int which, int *first)
{
    const uint8_t *p = item_bytes(pad, k) + ITEM_PICTURES + 4 * which;
    *first           = u16(p);
    return u16(p + 2);
}

static bool ended(const uint8_t *b, size_t size, size_t at)
{
    for (; at < size; ++at)
        if (!b[at]) return true;
    return false;
}

/* the blob read: its items made (pad->owned); false when it is no blob of these limits */
static bool parse(dgx_di_pad_t *pad, const uint8_t *b, size_t size)
{
    if (!b || size < HEADER || memcmp(b, "DGXP", 4) || b[4] != 1) return false;
    const int    count = b[5], shapes = u16(b + 6), points = u16(b + 8);
    const size_t strings = HEADER + (size_t)ITEM_BYTES * count + (size_t)SHAPE_BYTES * shapes + (size_t)POINT_BYTES * points;
    if (count < 1 || count > ITEMS_MAX || strings > size) return false;
    if (u16(b + 10) != NONE && !ended(b, size, strings + u16(b + 10))) return false;
    for (int k = 0; k < count; ++k) {
        const uint8_t *it = b + HEADER + ITEM_BYTES * k;
        if (it[0] > DGX_DI_PAD_STICK || it[1] > DGX_DI_FROM_END || it[2] > DGX_DI_FROM_END || i16(it + 8) < 1) return false;
        for (int w = 0; w < PICTURES; ++w) {
            const int first = u16(it + ITEM_PICTURES + 4 * w), n = u16(it + ITEM_PICTURES + 2 + 4 * w);
            if ((w >= pictures_of[it[0]] && n) || first + n > shapes) return false;
        }
    }
    for (int s = 0; s < shapes; ++s) {
        const uint8_t *sh = b + HEADER + ITEM_BYTES * count + SHAPE_BYTES * s;
        const int      first = u16(sh + 14), n = u16(sh + 16);
        if (sh[0] > LINE || sh[1] > DARK || i16(sh + 8) < 0 || i16(sh + 10) < 0 || i16(sh + 12) < 0 || first + n > points) return false;
        if ((sh[0] == POLYGON && n < 3) || (sh[0] == LINE && n < 2)) return false;
    }
    pad->owned = calloc((size_t)count, sizeof *pad->owned);
    if (!pad->owned) return false;
    for (int k = 0; k < count; ++k) {
        const uint8_t *it = b + HEADER + ITEM_BYTES * k;
        pad->owned[k] = (dgx_di_pad_item_t){.kind = it[0], .from_x = it[1], .from_y = it[2], .ways = it[3], .x = i16(it + 4), .y = i16(it + 6),
                                            .r = i16(it + 8), .out = u32(it + 12), .color = u16(it + 10)};
    }
    pad->blob = b, pad->count = count;
    return true;
}

/* --- pictures */

/* a shape's color: its own, or the item's, lighter, darker */
static uint16_t shape_color(const dgx_di_pad_t *pad, int k, const uint8_t *sh)
{
    const uint16_t item = color_of(pad, k);
    return sh[1] == ITEM_COLOR ? item : sh[1] == LIGHT ? lighter(item) : sh[1] == DARK ? darker(item) : u16(sh + 2);
}

/* where a shape draws, in pixels from the item's middle: left, top, right, bottom */
static void shape_box(const dgx_di_pad_t *pad, const uint8_t *sh, int box[4])
{
    const int x = px_x(pad, i16(sh + 4)), y = px_y(pad, i16(sh + 6)), w = px_x(pad, i16(sh + 8)), h = px_y(pad, i16(sh + 10)), r = px_r(pad, i16(sh + 12));
    if (sh[0] == CIRCLE || sh[0] == RING) box[0] = x - r, box[1] = y - r, box[2] = x + r, box[3] = y + r;
    else if (sh[0] == RECT) box[0] = x - w / 2, box[1] = y - h / 2, box[2] = x - w / 2 + w, box[3] = y - h / 2 + h;
    else {
        const int first = u16(sh + 14), n = u16(sh + 16), half = sh[0] == LINE ? (r + 1) / 2 : 0;
        box[0] = box[1] = INT16_MAX, box[2] = box[3] = INT16_MIN;
        for (int i = 0; i < n; ++i) {
            const uint8_t *p  = point_bytes(pad, first + i);
            const int      px = px_x(pad, i16(p)), py = px_y(pad, i16(p + 2));
            if (px - half < box[0]) box[0] = px - half;
            if (py - half < box[1]) box[1] = py - half;
            if (px + half > box[2]) box[2] = px + half;
            if (py + half > box[3]) box[3] = py + half;
        }
    }
}

static void rounded(dgx_screen_t *s, int x, int y, int w, int h, int r, uint32_t c)
{
    if (r > w / 2) r = w / 2;
    if (r > h / 2) r = h / 2;
    if (r < 1) {
        dgx_fill_rectangle(s, x, y, w, h, c);
        return;
    }
    dgx_fill_rectangle(s, x + r, y, w - 2 * r, h, c);
    dgx_fill_rectangle(s, x, y + r, r, h - 2 * r, c);
    dgx_fill_rectangle(s, x + w - r, y + r, r, h - 2 * r, c);
    dgx_solid_circle(s, x + r, y + r, r, c), dgx_solid_circle(s, x + w - 1 - r, y + r, r, c);
    dgx_solid_circle(s, x + r, y + h - 1 - r, r, c), dgx_solid_circle(s, x + w - 1 - r, y + h - 1 - r, r, c);
}

/* the four ways along the axes: a cross's arms, the buttons of a way */
static const struct {
    int      dx, dy;
    uint32_t way;
} way_of[4] = {{-1, 0, DGX_DI_PAD_LEFT}, {1, 0, DGX_DI_PAD_RIGHT}, {0, -1, DGX_DI_PAD_UP}, {0, 1, DGX_DI_PAD_DOWN}};

/* a rectangle from corner to corner, both in it */
static void bar(dgx_screen_t *s, int x0, int y0, int x1, int y1, uint32_t c)
{
    dgx_fill_rectangle(s, x0 < x1 ? x0 : x1, y0 < y1 ? y0 : y1, abs(x1 - x0) + 1, abs(y1 - y0) + 1, c);
}

/* a chevron: its point d from x, y the way dx, dy, its wings e back and e aside, by a pen of w */
static void chevron(dgx_screen_t *s, int x, int y, int dx, int dy, int d, int e, int w, uint32_t c)
{
    const int px = x + dx * d, py = y + dy * d, bx = px - dx * e, by = py - dy * e;
    dgx_draw_line_thick(s, bx - dy * e, by + dx * e, px, py, w, c);
    dgx_draw_line_thick(s, px, py, bx + dy * e, by - dx * e, w, c);
    if (w > 2) dgx_solid_circle(s, px, py, w / 2, c);
}

/* a cross's arm held the way dx, dy: from its end to the square of the middle (in each way), its end round by ri, its
 * inner side straight */
static void arm_held(dgx_screen_t *s, int x, int y, int dx, int dy, int in, int end, int ri, uint32_t c)
{
    const int x0 = dx < 0 ? x - end : dx > 0 ? x + in + 1 : x - in, y0 = dy < 0 ? y - end : dy > 0 ? y + in + 1 : y - in;
    const int w = dx ? end - in : 2 * in + 1, h = dy ? end - in : 2 * in + 1;
    rounded(s, x0, y0, w, h, ri, c);
    if (ri) dgx_fill_rectangle(s, dx < 0 ? x - in - ri : x0, dy < 0 ? y - in - ri : y0, dx ? ri : w, dy ? ri : h, c);
}

/* a shape drawn into the layer, its middle at cx, cy */
static void shape(dgx_di_pad_t *pad, int k, const uint8_t *sh, int cx, int cy)
{
    dgx_screen_t *l = pad->layer;
    const int     c = paint(pad, shape_color(pad, k, sh)), x = cx + px_x(pad, i16(sh + 4)), y = cy + px_y(pad, i16(sh + 6));
    const int     r = px_r(pad, i16(sh + 12)), first = u16(sh + 14), n = u16(sh + 16);
    switch (sh[0]) {
    case CIRCLE:
        dgx_solid_circle(l, x, y, r, c);
        break;
    case RING: {
        int w = px_r(pad, i16(sh + 8));
        if (w < 1) w = 1;
        if (w > r) w = r;
        dgx_draw_arc(l, x, y, r - w / 2, r - w / 2, 0, 360, w, c);
        break;
    }
    case RECT: {
        const int w = px_x(pad, i16(sh + 8)), h = px_y(pad, i16(sh + 10));
        rounded(l, x - w / 2, y - h / 2, w, h, r, c);
        break;
    }
    case POLYGON: {
        /* convex: a fan of triangles from its first point */
        const uint8_t *p0 = point_bytes(pad, first);
        const int      x0 = cx + px_x(pad, i16(p0)), y0 = cy + px_y(pad, i16(p0 + 2));
        for (int i = 1; i + 1 < n; ++i) {
            const uint8_t *p1 = point_bytes(pad, first + i), *p2 = point_bytes(pad, first + i + 1);
            dgx_draw_triangle_solid(l, x0, y0, cx + px_x(pad, i16(p1)), cy + px_y(pad, i16(p1 + 2)), cx + px_x(pad, i16(p2)), cy + px_y(pad, i16(p2 + 2)), c);
        }
        break;
    }
    case LINE: {
        /* point to point, round where they meet */
        const int w = r < 1 ? 1 : r;
        for (int i = 0; i + 1 < n; ++i) {
            const uint8_t *p1 = point_bytes(pad, first + i), *p2 = point_bytes(pad, first + i + 1);
            const int      x1 = cx + px_x(pad, i16(p1)), y1 = cy + px_y(pad, i16(p1 + 2)), x2 = cx + px_x(pad, i16(p2)), y2 = cy + px_y(pad, i16(p2 + 2));
            dgx_draw_line_thick(l, x1, y1, x2, y2, w, c);
            if (w > 2 && i + 2 < n) dgx_solid_circle(l, x2, y2, w / 2, c);
        }
        break;
    }
    }
}

/* a picture of an item drawn, its middle at cx, cy */
static void picture(dgx_di_pad_t *pad, int k, int which, int cx, int cy)
{
    int       first;
    const int n = picture_of(pad, k, which, &first);
    for (int i = 0; i < n; ++i) shape(pad, k, shape_bytes(pad, first + i), cx, cy);
}

/* the colors an item may draw, into the table; where it draws */
static void colors_and_box(dgx_di_pad_t *pad, int k)
{
    dgx_di_pad_place_t *pl = &pad->places[k];
    const int           r  = pl->r;
    pl->box[0] = (int16_t)(pl->x - r), pl->box[1] = (int16_t)(pl->y - r), pl->box[2] = (int16_t)(pl->x + r), pl->box[3] = (int16_t)(pl->y + r);
    if (!pl->pictured) {
        paint(pad, color_of(pad, k)), paint(pad, lighter(color_of(pad, k)));
        if (pad->items[k].kind == DGX_DI_PAD_CROSS) paint(pad, darker(color_of(pad, k))); /* its plate */
        return;
    }
    for (int w = 0; w < pictures_of[pad->items[k].kind]; ++w) {
        int       first;
        const int n = picture_of(pad, k, w, &first), travel = pad->items[k].kind == DGX_DI_PAD_STICK && w != WELL ? r / 2 : 0;
        for (int i = 0; i < n; ++i) {
            const uint8_t *sh = shape_bytes(pad, first + i);
            int            box[4];
            paint(pad, shape_color(pad, k, sh));
            shape_box(pad, sh, box);
            if (pl->x + box[0] - travel - 1 < pl->box[0]) pl->box[0] = (int16_t)(pl->x + box[0] - travel - 1);
            if (pl->y + box[1] - travel - 1 < pl->box[1]) pl->box[1] = (int16_t)(pl->y + box[1] - travel - 1);
            if (pl->x + box[2] + travel + 1 > pl->box[2]) pl->box[2] = (int16_t)(pl->x + box[2] + travel + 1);
            if (pl->y + box[3] + travel + 1 > pl->box[3]) pl->box[3] = (int16_t)(pl->y + box[3] + travel + 1);
        }
    }
}

/* an item as it is now painted into the layer, over what is there: by its pictures, or as by default */
static void paint_item(dgx_di_pad_t *pad, int k)
{
    const dgx_di_pad_item_t  *it = &pad->items[k];
    const dgx_di_pad_place_t *pl = &pad->places[k];
    dgx_screen_t             *l  = pad->layer;
    const int                 x = pl->x, y = pl->y, r = pl->r;
    const uint32_t            s = pl->drawn;
    if (pl->pictured) {
        switch (it->kind) {
        case DGX_DI_PAD_BUTTON:
            picture(pad, k, s ? 1 : 0, x, y);
            break;
        case DGX_DI_PAD_CROSS:
            picture(pad, k, 0, x, y);
            if (s & DGX_DI_PAD_UP) picture(pad, k, ARM_UP, x, y);
            if (s & DGX_DI_PAD_DOWN) picture(pad, k, ARM_DOWN, x, y);
            if (s & DGX_DI_PAD_LEFT) picture(pad, k, ARM_LEFT, x, y);
            if (s & DGX_DI_PAD_RIGHT) picture(pad, k, ARM_RIGHT, x, y);
            if ((s & (DGX_DI_PAD_UP | DGX_DI_PAD_LEFT)) == (DGX_DI_PAD_UP | DGX_DI_PAD_LEFT)) picture(pad, k, UP_LEFT, x, y);
            if ((s & (DGX_DI_PAD_UP | DGX_DI_PAD_RIGHT)) == (DGX_DI_PAD_UP | DGX_DI_PAD_RIGHT)) picture(pad, k, UP_RIGHT, x, y);
            if ((s & (DGX_DI_PAD_DOWN | DGX_DI_PAD_LEFT)) == (DGX_DI_PAD_DOWN | DGX_DI_PAD_LEFT)) picture(pad, k, DOWN_LEFT, x, y);
            if ((s & (DGX_DI_PAD_DOWN | DGX_DI_PAD_RIGHT)) == (DGX_DI_PAD_DOWN | DGX_DI_PAD_RIGHT)) picture(pad, k, DOWN_RIGHT, x, y);
            break;
        case DGX_DI_PAD_STICK: {
            int dx, dy;
            knob_of(s, &dx, &dy);
            picture(pad, k, WELL, x, y);
            picture(pad, k, s ? KNOB_HELD : KNOB, x + dx, y + dy);
            break;
        }
        }
    } else {
        const int rim = paint(pad, rim_color()), sign = paint(pad, sign_color());
        const int plain = paint(pad, color_of(pad, k)), held = paint(pad, lighter(color_of(pad, k)));
        switch (it->kind) {
        case DGX_DI_PAD_BUTTON: {
            dgx_solid_circle(l, x, y, r, s ? held : plain);
            dgx_draw_circle(l, x, y, r, rim), dgx_draw_circle(l, x, y, r - 1, rim);
            const int q = r / 3, pen = r / 12 > 3 ? r / 12 : 3;
            if (it->out == DGX_DI_PAD_START) dgx_draw_triangle_solid(l, x - q, y - q, x - q, y + q, x + q, y, sign);
            if (it->out == DGX_DI_PAD_BACK) dgx_draw_triangle_solid(l, x + q, y - q, x + q, y + q, x - q, y, sign);
            if (it->out == DGX_DI_PAD_HOME) dgx_fill_rectangle(l, x - q, y - q, 2 * q + 1, 2 * q + 1, sign);
            /* a button of a way: its chevron, as a cross's */
            for (int i = 0; i < 4; ++i)
                if (it->out == way_of[i].way) chevron(l, x, y, way_of[i].dx, way_of[i].dy, r * 15 / 100, r * 3 / 10, pen, sign);
            break;
        }
        case DGX_DI_PAD_CROSS: {
            /*
             * as the flat skin of Delta has one: on a round plate, darker, all of it a finger takes, a plus with a light
             * rim and round corners, its arms held lighter, a chevron on each, white while held, a ring round its middle
             * where a finger takes no way; a bracket in each corner between two arms, white when that diagonal is held
             * (none on a cross of four ways)
             */
            const int a = arm(r), len = r * 92 / 100, t = r / 45 > 2 ? r / 45 : 2, pen = r / 22 > 3 ? r / 22 : 3;
            const int round = a * 45 / 100, ri = round > t ? round - t : 0, in = a - t, end = len - t, middle = r * 3 / 20;
            dgx_solid_circle(l, x, y, r, paint(pad, darker(color_of(pad, k))));
            if (!(it->ways & DGX_DI_PAD_FOUR)) {
                const int A = r * 60 / 100, L = r * 13 / 100, soft = paint(pad, softer(color_of(pad, k)));
                for (int c = 0; c < 4; ++c) {
                    const int      sx = c & 1 ? 1 : -1, sy = c & 2 ? 1 : -1, px = x + sx * A, py = y + sy * A;
                    const uint32_t both = (sx < 0 ? DGX_DI_PAD_LEFT : DGX_DI_PAD_RIGHT) | (sy < 0 ? DGX_DI_PAD_UP : DGX_DI_PAD_DOWN);
                    const int      ink = (s & both) == both ? sign : soft, lo = pen / 2, hi = pen - 1 - lo;
                    /* its legs along the axes, back from its corner, square */
                    bar(l, px - sx * L, py - lo, px + (sx > 0 ? hi : -lo), py + hi, ink);
                    bar(l, px - lo, py - sy * L, px + hi, py + (sy > 0 ? hi : -lo), ink);
                }
            }
            rounded(l, x - len, y - a, 2 * len + 1, 2 * a + 1, round, rim);
            rounded(l, x - a, y - len, 2 * a + 1, 2 * len + 1, round, rim);
            rounded(l, x - end, y - in, 2 * end + 1, 2 * in + 1, ri, plain);
            rounded(l, x - in, y - end, 2 * in + 1, 2 * end + 1, ri, plain);
            for (int i = 0; i < 4; ++i)
                if (s & way_of[i].way) arm_held(l, x, y, way_of[i].dx, way_of[i].dy, in, end, ri, held);
            for (int i = 0; i < 4; ++i) chevron(l, x, y, way_of[i].dx, way_of[i].dy, len * 78 / 100, a * 42 / 100, pen, s & way_of[i].way ? sign : rim);
            dgx_draw_arc(l, x, y, middle - t / 2, middle - t / 2, 0, 360, t, rim);
            break;
        }
        case DGX_DI_PAD_STICK: {
            /* its well, its knob where the finger has it, half the way (it stays inside the well) */
            int dx, dy;
            knob_of(s, &dx, &dy);
            dgx_solid_circle(l, x, y, r, plain);
            dgx_draw_circle(l, x, y, r, rim);
            dgx_solid_circle(l, x + dx, y + dy, r / 2, s ? held : plain);
            dgx_draw_circle(l, x + dx, y + dy, r / 2, rim);
            break;
        }
        }
    }
}

static bool meet(const int16_t *a, const int16_t *b)
{
    return a[0] <= b[2] && b[0] <= a[2] && a[1] <= b[3] && b[1] <= a[3];
}

/*
 * an item drawn as it is now: one by its pictures, what it drew before taken away first (its box), and what the box
 * took of the others drawn again, in their order; one as by default over itself
 */
static void draw(dgx_di_pad_t *pad, int k)
{
    dgx_di_pad_place_t *pl = &pad->places[k];
    dgx_screen_progress_up(pad->layer);
    pl->drawn = pl->state;
    if (pl->pictured) {
        dgx_fill_rectangle(pad->layer, pl->box[0], pl->box[1], pl->box[2] - pl->box[0] + 1, pl->box[3] - pl->box[1] + 1, EMPTY);
        for (int j = 0; j < pad->count; ++j)
            if (j == k || meet(pad->places[j].box, pl->box)) paint_item(pad, j);
    } else
        paint_item(pad, k);
    dgx_screen_progress_down(pad->layer);
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

/* the pad of its items made: places, the table of colors, the mask and the layer drawn */
static bool make(dgx_di_pad_t *pad, const dgx_screen_t *display, int width_mm, int height_mm)
{
    const int w = display->width, h = display->height, count = pad->count;
    pad->width = (int16_t)w, pad->height = (int16_t)h, pad->mm_w = (int16_t)width_mm, pad->mm_h = (int16_t)height_mm;
    pad->places  = calloc((size_t)count, sizeof *pad->places);
    pad->palette = calloc(256, sizeof *pad->palette);
    pad->mask    = calloc((size_t)w * h, 1);
    pad->layer   = dgx_vscreen_init(w, h, 8, DgxScreenRGB);
    if (!pad->places || !pad->palette || !pad->mask || !pad->layer) {
        dgx_di_pad_free(pad, NULL);
        return false;
    }
    /* none first, the rim and the signs of the default look next */
    pad->colors = 1;
    paint(pad, rim_color()), paint(pad, sign_color());
    for (int k = 0; k < count; ++k) {
        const dgx_di_pad_item_t *it = &pad->items[k];
        /* a radius by both ways of the display, so that it stays round on pixels not square */
        const int r = (pixels(it->r, w, width_mm) + pixels(it->r, h, height_mm) + 1) / 2;
        pad->places[k] = (dgx_di_pad_place_t){.x = (int16_t)place_of(it->from_x, pixels(it->x, w, width_mm), w),
                                              .y = (int16_t)place_of(it->from_y, pixels(it->y, h, height_mm), h), .r = (int16_t)(r < 2 ? 2 : r)};
        if (pad->blob) {
            const uint8_t *ib = item_bytes(pad, k);
            pad->places[k].pictured = ib[ITEM_PICTURED];
        }
        colors_and_box(pad, k);
        mark(pad, k);
    }
    /* drawn once all are placed: one drawn again draws what it takes of the others */
    for (int k = 0; k < count; ++k) draw(pad, k);
    return true;
}

bool dgx_di_pad_init(dgx_di_pad_t *pad, const dgx_screen_t *display, int width_mm, int height_mm, const dgx_di_pad_item_t *items, int count)
{
    memset(pad, 0, sizeof *pad);
    for (int i = 0; i < DGX_DI_FINGERS; ++i) pad->of[i] = -1;
    if (!display || width_mm <= 0 || height_mm <= 0 || !items || count <= 0 || count > ITEMS_MAX) return false;
    pad->items = items, pad->count = count;
    return make(pad, display, width_mm, height_mm);
}

bool dgx_di_pad_open(dgx_di_pad_t *pad, const dgx_screen_t *display, int width_mm, int height_mm, const uint8_t *layout, size_t size)
{
    memset(pad, 0, sizeof *pad);
    for (int i = 0; i < DGX_DI_FINGERS; ++i) pad->of[i] = -1;
    if (!display || width_mm <= 0 || height_mm <= 0 || !parse(pad, layout, size)) return false;
    pad->items = pad->owned;
    return make(pad, display, width_mm, height_mm);
}

void dgx_di_pad_free(dgx_di_pad_t *pad, dgx_screen_t *display)
{
    if (display && pad->layer) dgx_di_pad_hide(pad, display);
    if (pad->layer) dgx_screen_destroy(&pad->layer);
    free(pad->mask), free(pad->places), free(pad->palette), free(pad->owned);
    pad->mask = NULL, pad->places = NULL, pad->palette = NULL, pad->owned = NULL, pad->count = 0, pad->colors = 0;
}

bool dgx_di_pad_show(dgx_di_pad_t *pad, dgx_screen_t *display, uint8_t alpha)
{
    uint16_t lut[256] = {0};
    for (int i = 1; i < pad->colors; ++i) lut[i] = pad->palette[i];
    return pad->layer && dgx_overlay_set(display, pad->layer, 0, 0, lut, alpha);
}

void dgx_di_pad_hide(dgx_di_pad_t *pad, dgx_screen_t *display)
{
    /* the display's overlay this pad's layer (the two share it); another's is left */
    if (pad->layer && display->overlay && display->overlay == pad->layer->overlay) dgx_overlay_clear(display);
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
