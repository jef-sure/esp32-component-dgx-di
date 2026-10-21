/*
 * A pad on the display: its items placed from millimetres, its mask and layer; a finger down on an item the pad's
 * (claimed: not the program's first finger), a button held within half its radius again, a cross as it is drawn
 * (none at the middle, an arm its way, a corner between two arms both), a stick by how far (fixed, floating); several fingers;
 * an item pressed drawn so, the others not; laid over the display and taken away. Of blobs (the ready layouts of
 * tools/pad-editor/examples, made by tools/pad_blob.py): the classic one as the C one, layer and all; pictures of their
 * own — a button's normal and pressed, a sign, a cross's arm held over it, a stick's knob where the finger has it and
 * the well under it; blobs not of the format refused. On a display of 1280x800 over 217x136 mm, the 10" Guition's.
 */
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "check.h"
#include "dgx_di.h"
#include "dgx_di_pad.h"
#include "dgx_overlay.h"
#include "drivers/vscreen.h"

typedef struct {
    dgx_di_device_t device;
    int             n;
    dgx_di_point_t  points[4];
} fake_t;

static int fake_read(dgx_di_device_t *device, dgx_di_point_t *points, int max)
{
    const fake_t *f = (const fake_t *)device;
    const int     n = f->n < max ? f->n : max;
    memcpy(points, f->points, sizeof *points * (size_t)n);
    return n;
}

enum { W = 1280, H = 800, MM_W = 217, MM_H = 136 };

static fake_t           f = {.device = {.read = fake_read, .fingers = 4, .x_min = 0, .x_max = W - 1, .y_min = 0, .y_max = H - 1}};
static dgx_di_pointer_t p;
static uint32_t         now;

/* fingers at these places (id, x, y), or none; a read 10 ms later, the pad taken */
static void look(dgx_di_pad_t *pad, int n, const int (*at)[3])
{
    f.n = n;
    for (int i = 0; i < n; ++i) f.points[i] = (dgx_di_point_t){.x = (int16_t)at[i][1], .y = (int16_t)at[i][2], .id = (uint8_t)at[i][0]};
    now += 10;
    dgx_di_pointer_read(&p, now);
    dgx_di_pad_take(pad, &p);
}

static void one(dgx_di_pad_t *pad, int x, int y)
{
    const int at[1][3] = {{1, x, y}};
    look(pad, 1, at);
}

static void none(dgx_di_pad_t *pad)
{
    look(pad, 0, NULL);
}

/* the layer's color index at x, y; its color */
static unsigned shown(const dgx_di_pad_t *pad, int x, int y)
{
    return (unsigned)pad->layer->get_pixel(pad->layer, x, y);
}

static uint16_t color_at(const dgx_di_pad_t *pad, int x, int y)
{
    return pad->palette[shown(pad, x, y)];
}

/* a blob of one item in the middle of the display and its pictures, made by hand as tools/pad_blob.py makes one */
typedef struct {
    uint8_t picture, shape, paint;
    int16_t r, w;
} hand_shape_t;

static void put16(uint8_t *p, int v)
{
    p[0] = (uint8_t)(v & 255), p[1] = (uint8_t)((v >> 8) & 255);
}

static size_t blob_of(uint8_t *b, int kind, int r, uint16_t color, const hand_shape_t *sh, int n)
{
    memset(b, 0, 16 + 56 + 20 * (size_t)n);
    memcpy(b, "DGXP", 4), b[4] = 1, b[5] = 1, put16(b + 6, n), put16(b + 10, 0xffff);
    uint8_t *it = b + 16;
    it[0] = (uint8_t)kind, it[1] = it[2] = DGX_DI_FROM_MIDDLE, put16(it + 8, r), put16(it + 10, color), it[12] = kind == DGX_DI_PAD_BUTTON ? 0x10 : 0, it[52] = 1;
    for (int i = 0; i < n; ++i) {
        uint8_t *shape = b + 16 + 56 + 20 * i, *slot = it + 16 + 4 * sh[i].picture;
        if (!slot[2] && !slot[3]) put16(slot, i);
        put16(slot + 2, slot[2] + 1);
        shape[0] = sh[i].shape, shape[1] = sh[i].paint, put16(shape + 8, sh[i].w), put16(shape + 12, sh[i].r);
    }
    return 16 + 56 + 20 * (size_t)n;
}

/* a pressed color, as the pad makes it: half way to white; a cross's plate: half way to black; its brackets at rest: a
 * quarter of the way to white */
static uint16_t lighter_of(uint16_t c)
{
    const int r = (c >> 11) & 31, g = (c >> 5) & 63, b = c & 31;
    return (uint16_t)(((r + 31) / 2) << 11 | ((g + 63) / 2) << 5 | (b + 31) / 2);
}
static uint16_t darker_of(uint16_t c)
{
    const int r = (c >> 11) & 31, g = (c >> 5) & 63, b = c & 31;
    return (uint16_t)((r / 2) << 11 | (g / 2) << 5 | b / 2);
}
static uint16_t softer_of(uint16_t c)
{
    const int r = (c >> 11) & 31, g = (c >> 5) & 63, b = c & 31;
    return (uint16_t)(((3 * r + 31) / 4) << 11 | ((3 * g + 63) / 4) << 5 | (3 * b + 31) / 4);
}
enum { GREY = 0x5ACB, RIM = 0xE71C }; /* an item of no color of its own; the rims and the cross's chevrons at rest */

int main(void)
{
    dgx_screen_t *display = dgx_vscreen_init(W, H, 16, DgxScreenRGB);
    dgx_di_pointer_init(&p, &f.device, 0, display);
    dgx_di_pad_t pad;
    CHECK(dgx_di_pad_init(&pad, display, MM_W, MM_H, dgx_di_pad_classic, DGX_DI_PAD_CLASSIC));
    const dgx_di_pad_place_t *cross = &pad.places[0], *a = &pad.places[1], *b = &pad.places[2], *start = &pad.places[6];

    /* placed from millimetres: the cross 30 mm from the left and the bottom, 20 mm round; A 30 from the right, 22 up, 7 */
    CHECK(cross->x == 177 && cross->y == H - 1 - 176 && cross->r == 118);
    CHECK(a->x == W - 1 - 177 && a->y == H - 1 - 129 && a->r == 41);
    CHECK(start->x == W / 2 + 88 && start->r == 29);
    /* the mask: each item where it is, none elsewhere */
    CHECK(dgx_di_pad_item_at(&pad, a->x, a->y) == 1 && dgx_di_pad_item_at(&pad, a->x + a->r, a->y) == 1);
    CHECK(dgx_di_pad_item_at(&pad, a->x + a->r + 1, a->y) == -1 && dgx_di_pad_item_at(&pad, 0, 0) == -1 && dgx_di_pad_item_at(&pad, -1, 5) == -1 &&
          dgx_di_pad_item_at(&pad, W, 5) == -1);
    CHECK(dgx_di_pad_item_at(&pad, cross->x, cross->y) == 0);
    /* drawn: A in its own color, nothing beside it */
    const unsigned a_color = shown(&pad, a->x, a->y);
    CHECK(a_color != 0 && shown(&pad, 0, 0) == 0 && shown(&pad, b->x, b->y) != a_color);

    /* A pressed: its bit, the finger claimed (the program's first finger none), A drawn lighter, B as it was */
    int        x, y;
    const unsigned b_color = shown(&pad, b->x, b->y);
    one(&pad, a->x + 5, a->y - 5);
    CHECK(pad.buttons == DGX_DI_PAD_A && pad.pressed == DGX_DI_PAD_A && pad.released == 0);
    CHECK(p.fingers[0].claimed && !dgx_di_pointer_first(&p, &x, &y));
    CHECK(color_at(&pad, a->x, a->y) == lighter_of(pad.palette[a_color]) && shown(&pad, b->x, b->y) == b_color);
    /* held within half its radius again, led farther: let go, the finger still the pad's (over B: not B) */
    one(&pad, a->x + a->r * 14 / 10, a->y);
    CHECK(pad.buttons == DGX_DI_PAD_A && pad.pressed == 0);
    one(&pad, a->x + a->r * 16 / 10, a->y);
    CHECK(pad.buttons == 0 && pad.released == DGX_DI_PAD_A && shown(&pad, a->x, a->y) == a_color);
    one(&pad, b->x, b->y);
    CHECK(pad.buttons == 0 && !dgx_di_pointer_first(&p, &x, &y));
    none(&pad);
    CHECK(pad.buttons == 0 && pad.released == 0);

    /* two at once: A and B; one up, the other held */
    {
        const int two[2][3] = {{1, a->x, a->y}, {2, b->x, b->y}};
        look(&pad, 2, two);
        CHECK(pad.buttons == (DGX_DI_PAD_A | DGX_DI_PAD_B) && pad.pressed == (DGX_DI_PAD_A | DGX_DI_PAD_B));
        const int left[1][3] = {{2, b->x, b->y}};
        look(&pad, 1, left);
        CHECK(pad.buttons == DGX_DI_PAD_B && pad.released == DGX_DI_PAD_A);
        none(&pad);
    }
    /* beside the items: none's, the program's first finger */
    one(&pad, W / 2, 100);
    CHECK(pad.buttons == 0 && !p.fingers[0].claimed && dgx_di_pointer_first(&p, &x, &y) && x == W / 2);
    none(&pad);

    /* the cross as drawn (arms 0.35 of its radius wide each way): none at its middle, an arm its way, a corner both */
    {
        const int r = cross->r;
        static const struct {
            int      dx, dy; /* hundredths of its radius */
            uint32_t ways;
        } at[] = {
            {10, 0, 0},                                         /* the middle */
            {30, 25, DGX_DI_PAD_RIGHT},                         /* where the arms cross: the larger way */
            {20, -40, DGX_DI_PAD_UP},                           /* on the upper arm */
            {80, 0, DGX_DI_PAD_RIGHT},  {-80, 0, DGX_DI_PAD_LEFT}, {0, -80, DGX_DI_PAD_UP}, {0, 80, DGX_DI_PAD_DOWN},
            {60, 60, DGX_DI_PAD_RIGHT | DGX_DI_PAD_DOWN},      /* a corner between two arms: both */
            {-70, -70, DGX_DI_PAD_LEFT | DGX_DI_PAD_UP},
            {-40, 40, DGX_DI_PAD_LEFT | DGX_DI_PAD_DOWN},      /* the inner corner (the 7" asked for it first) */
            {80, 35, DGX_DI_PAD_RIGHT},                         /* on the arm's edge: the arm */
            {80, 40, DGX_DI_PAD_RIGHT | DGX_DI_PAD_DOWN},       /* just past it: the corner */
            {200, -10, DGX_DI_PAD_RIGHT},                       /* out of it: still the cross's */
        };
        one(&pad, cross->x, cross->y);
        CHECK(pad.buttons == 0 && p.fingers[0].claimed);
        for (size_t i = 0; i < sizeof at / sizeof *at; ++i) {
            one(&pad, cross->x + at[i].dx * r / 100, cross->y + at[i].dy * r / 100);
            if (pad.buttons != at[i].ways) fprintf(stderr, "the cross at %d, %d of 100: %x, not %x\n", at[i].dx, at[i].dy, pad.buttons, at[i].ways);
            CHECK(pad.buttons == at[i].ways);
        }
        CHECK(shown(&pad, cross->x + r / 2, cross->y) != shown(&pad, cross->x - r / 2, cross->y)); /* the arm held lighter */
        none(&pad);
        CHECK(pad.buttons == 0 && shown(&pad, cross->x + r / 2, cross->y) == shown(&pad, cross->x - r / 2, cross->y));
        /*
         * as the flat skin of Delta: its plate, darker, all of it a finger takes; a plus of its grey with a light rim; a
         * chevron on each arm, white while held; a ring round its middle; a bracket in each corner, white when that
         * diagonal is held
         */
        const uint16_t plate = darker_of(GREY);
        const int      len = r * 92 / 100, d = len * 78 / 100, A = r * 60 / 100, L = r * 13 / 100, middle = r * 3 / 20;
        const int      px = cross->x + r * 8 / 10, py = cross->y + r * 4 / 10, bx = cross->x - A, by = cross->y + A - L / 2;
        CHECK(color_at(&pad, px, py) == plate && color_at(&pad, cross->x + len, cross->y) == RIM && color_at(&pad, cross->x + len + 2, cross->y) == plate);
        CHECK(color_at(&pad, cross->x + r / 2, cross->y) == GREY && color_at(&pad, cross->x + d, cross->y) == RIM && color_at(&pad, cross->x, cross->y - d) == RIM);
        CHECK(color_at(&pad, cross->x + middle - 1, cross->y) == RIM && color_at(&pad, cross->x, cross->y) == GREY);
        CHECK(color_at(&pad, bx, by) == softer_of(GREY));
        one(&pad, cross->x - r * 6 / 10, cross->y + r * 6 / 10);
        CHECK(pad.buttons == (DGX_DI_PAD_LEFT | DGX_DI_PAD_DOWN) && color_at(&pad, bx, by) == 0xFFFF && color_at(&pad, cross->x + A, cross->y + A - L / 2) == softer_of(GREY));
        CHECK(color_at(&pad, cross->x - d, cross->y) == 0xFFFF && color_at(&pad, cross->x, cross->y + d) == 0xFFFF && color_at(&pad, cross->x + d, cross->y) == RIM);
        CHECK(color_at(&pad, cross->x - r / 2, cross->y) == lighter_of(GREY) && color_at(&pad, cross->x, cross->y + r / 2) == lighter_of(GREY) && color_at(&pad, px, py) == plate);
        const int in = r * 7 / 20 - 2; /* the inner side of an arm held: straight, to the square of the middle */
        CHECK(color_at(&pad, cross->x - in - 1, cross->y - in) == lighter_of(GREY) && color_at(&pad, cross->x - in, cross->y - in) == GREY);
        none(&pad);
        CHECK(color_at(&pad, bx, by) == softer_of(GREY) && color_at(&pad, cross->x - d, cross->y) == RIM && color_at(&pad, cross->x - r / 2, cross->y) == GREY);
    }

    /* a pad of four ways only: no two at once */
    {
        static const dgx_di_pad_item_t four[] = {{DGX_DI_PAD_CROSS, DGX_DI_FROM_MIDDLE, DGX_DI_FROM_MIDDLE, DGX_DI_PAD_FOUR, 0, 0, 200, 0, 0}};
        dgx_di_pad_t                   q;
        CHECK(dgx_di_pad_init(&q, display, MM_W, MM_H, four, 1));
        const dgx_di_pad_place_t *c = &q.places[0];
        one(&q, c->x, c->y), one(&q, c->x + c->r * 6 / 10, c->y + c->r * 7 / 10);
        CHECK(q.buttons == DGX_DI_PAD_DOWN);
        const int A = c->r * 60 / 100, L = c->r * 13 / 100;
        CHECK(color_at(&q, c->x - A, c->y + A - L / 2) == darker_of(GREY)); /* no brackets: no diagonals */
        none(&q);
        dgx_di_pad_free(&q, NULL);
    }

    /* buttons of the ways: a chevron each, its point the way, white on the button's grey, open behind its point */
    {
        static const dgx_di_pad_item_t ways[] = {
            {DGX_DI_PAD_BUTTON, DGX_DI_FROM_START, DGX_DI_FROM_MIDDLE, 0, 200, 0, 100, DGX_DI_PAD_LEFT, 0},
            {DGX_DI_PAD_BUTTON, DGX_DI_FROM_START, DGX_DI_FROM_MIDDLE, 0, 450, 0, 100, DGX_DI_PAD_RIGHT, 0},
            {DGX_DI_PAD_BUTTON, DGX_DI_FROM_END, DGX_DI_FROM_MIDDLE, 0, 450, 0, 100, DGX_DI_PAD_UP, 0},
            {DGX_DI_PAD_BUTTON, DGX_DI_FROM_END, DGX_DI_FROM_MIDDLE, 0, 200, 0, 100, DGX_DI_PAD_DOWN, 0},
        };
        static const int to[4][2] = {{-1, 0}, {1, 0}, {0, -1}, {0, 1}};
        dgx_di_pad_t     q;
        CHECK(dgx_di_pad_init(&q, display, MM_W, MM_H, ways, 4));
        for (int k = 0; k < 4; ++k) {
            const dgx_di_pad_place_t *b = &q.places[k];
            const int                 dx = to[k][0], dy = to[k][1], d = b->r * 15 / 100, e = b->r * 3 / 10;
            const int                 px = b->x + dx * d, py = b->y + dy * d, wx = px - dx * e - dy * e, wy = py - dy * e + dx * e;
            CHECK(color_at(&q, px, py) == 0xFFFF);                               /* its point */
            CHECK(color_at(&q, (3 * wx + px) / 4, (3 * wy + py) / 4) == 0xFFFF); /* a wing, near its end */
            CHECK(color_at(&q, b->x, b->y) == GREY);                             /* open behind its point */
            CHECK(color_at(&q, b->x - dx * d, b->y - dy * d) == GREY);           /* no point the other way */
        }
        dgx_di_pad_free(&q, NULL);
    }

    /* sticks: the left one fixed, the right one floating; their axes, whole travel at the rim, past it the same */
    {
        static const dgx_di_pad_item_t sticks[] = {
            {DGX_DI_PAD_STICK, DGX_DI_FROM_START, DGX_DI_FROM_MIDDLE, 0, 400, 0, 150, 0, 0},
            {DGX_DI_PAD_STICK, DGX_DI_FROM_END, DGX_DI_FROM_MIDDLE, DGX_DI_PAD_FLOATING, 400, 0, 150, 1, 0},
        };
        dgx_di_pad_t q;
        CHECK(dgx_di_pad_init(&q, display, MM_W, MM_H, sticks, 2));
        const dgx_di_pad_place_t *l = &q.places[0], *rr = &q.places[1];
        one(&q, l->x, l->y);
        CHECK(q.axes[DGX_DI_AXIS_LX] == 0 && q.axes[DGX_DI_AXIS_LY] == 0);
        one(&q, l->x + l->r, l->y);
        CHECK(q.axes[DGX_DI_AXIS_LX] == DGX_DI_AXIS_MAX && q.axes[DGX_DI_AXIS_LY] == 0);
        one(&q, l->x, l->y - l->r / 2);
        CHECK(abs(q.axes[DGX_DI_AXIS_LY] + DGX_DI_AXIS_MAX / 2) < 400 && q.axes[DGX_DI_AXIS_LX] == 0);
        one(&q, l->x + 3 * l->r, l->y + 3 * l->r); /* far past the rim, 45 degrees: 0.707 each way */
        CHECK(abs(q.axes[DGX_DI_AXIS_LX] - 23170) < 200 && abs(q.axes[DGX_DI_AXIS_LY] - 23170) < 200);
        none(&q);
        CHECK(q.axes[DGX_DI_AXIS_LX] == 0 && q.axes[DGX_DI_AXIS_LY] == 0);
        /* floating: from where it went down, wherever on the stick */
        one(&q, rr->x - rr->r / 2, rr->y + rr->r / 2);
        CHECK(q.axes[DGX_DI_AXIS_RX] == 0 && q.axes[DGX_DI_AXIS_RY] == 0);
        one(&q, rr->x - rr->r / 2 + rr->r / 2, rr->y + rr->r / 2);
        CHECK(abs(q.axes[DGX_DI_AXIS_RX] - DGX_DI_AXIS_MAX / 2) < 400 && q.axes[DGX_DI_AXIS_RY] == 0 && q.axes[DGX_DI_AXIS_LX] == 0);
        /* both at once, each its own */
        const int two[2][3] = {{1, rr->x, rr->y + rr->r / 2}, {2, l->x - l->r, l->y}};
        look(&q, 2, two);
        CHECK(q.axes[DGX_DI_AXIS_LX] == -DGX_DI_AXIS_MAX && abs(q.axes[DGX_DI_AXIS_RX] - DGX_DI_AXIS_MAX / 2) < 400);
        none(&q);
        dgx_di_pad_free(&q, NULL);
    }

    /* pixels not square (1280x800 over 217x200 mm): the radius by both ways, the places each by its own */
    {
        dgx_di_pad_t q;
        CHECK(dgx_di_pad_init(&q, display, MM_W, 200, dgx_di_pad_classic, DGX_DI_PAD_CLASSIC));
        CHECK(q.places[0].r == (118 + 80 + 1) / 2 && q.places[0].x == 177 && q.places[0].y == H - 1 - 120);
        dgx_di_pad_free(&q, NULL);
    }

    /* of a blob: the classic one as the C one, place for place, color for color, pixel for pixel */
    {
        dgx_di_pad_t q;
        CHECK(dgx_di_pad_open(&q, display, MM_W, MM_H, dgx_di_pad_layout_classic, dgx_di_pad_layout_classic_size));
        CHECK(q.count == DGX_DI_PAD_CLASSIC && q.colors == pad.colors && q.owned && q.items == q.owned);
        for (int k = 0; k < q.count; ++k)
            CHECK(q.places[k].x == pad.places[k].x && q.places[k].y == pad.places[k].y && q.places[k].r == pad.places[k].r && !q.places[k].pictured);
        CHECK(!memcmp(q.palette, pad.palette, sizeof *q.palette * (size_t)q.colors));
        bool same = true;
        for (int yy = 0; yy < H && same; ++yy)
            for (int xx = 0; xx < W && same; ++xx) same = shown(&q, xx, yy) == shown(&pad, xx, yy);
        CHECK(same);
        dgx_di_pad_free(&q, NULL);
        CHECK(!q.owned && !q.palette && !q.mask);
    }
    /* flat: its own pictures — A of its color, pressed lighter, back again; START's sign white; the cross's arm held lighter over the arm, the other not */
    {
        dgx_di_pad_t q;
        CHECK(dgx_di_pad_open(&q, display, MM_W, MM_H, dgx_di_pad_layout_flat, dgx_di_pad_layout_flat_size));
        const dgx_di_pad_place_t *qa = &q.places[1], *qs = &q.places[6], *qc = &q.places[0];
        CHECK(qa->pictured && qc->pictured && color_at(&q, qa->x, qa->y) == 0x2E65);
        one(&q, qa->x, qa->y);
        CHECK(q.buttons == DGX_DI_PAD_A && color_at(&q, qa->x, qa->y) == lighter_of(0x2E65));
        none(&q);
        CHECK(color_at(&q, qa->x, qa->y) == 0x2E65);
        CHECK(color_at(&q, qs->x, qs->y) == 0xFFFF && color_at(&q, qs->x + qs->r * 8 / 10, qs->y) != 0xFFFF);
        const int      ax = qc->x + qc->r / 2, bx = qc->x - qc->r / 2;
        const uint16_t arm_color = color_at(&q, ax, qc->y);
        CHECK(arm_color && arm_color == color_at(&q, bx, qc->y));
        one(&q, ax, qc->y);
        CHECK(q.buttons == DGX_DI_PAD_RIGHT && color_at(&q, ax, qc->y) == lighter_of(arm_color) && color_at(&q, bx, qc->y) == arm_color);
        CHECK(color_at(&q, qc->x + qc->r * 72 / 100, qc->y) == 0xFFFF); /* its chevron over the arm held */
        none(&q);
        CHECK(color_at(&q, ax, qc->y) == arm_color);
        /* its corner held: both arms and the corner's own picture, its bracket white */
        const int A = qc->r * 60 / 100, L = qc->r * 13 / 100;
        CHECK(color_at(&q, qc->x - A, qc->y + A - L / 2) == lighter_of(arm_color));
        one(&q, qc->x - qc->r * 6 / 10, qc->y + qc->r * 6 / 10);
        CHECK(q.buttons == (DGX_DI_PAD_LEFT | DGX_DI_PAD_DOWN) && color_at(&q, qc->x - A, qc->y + A - L / 2) == 0xFFFF);
        CHECK(color_at(&q, qc->x + A, qc->y + A - L / 2) == lighter_of(arm_color) && color_at(&q, bx, qc->y) == lighter_of(arm_color));
        none(&q);
        CHECK(color_at(&q, qc->x - A, qc->y + A - L / 2) == lighter_of(arm_color));
        dgx_di_pad_free(&q, NULL);
    }
    /* twin: a floating stick of glass — its knob in the middle at rest; held a radius to the right, the knob half way, lighter, the well where it was */
    {
        dgx_di_pad_t q;
        CHECK(dgx_di_pad_open(&q, display, MM_W, MM_H, dgx_di_pad_layout_twin, dgx_di_pad_layout_twin_size));
        const dgx_di_pad_place_t *st = &q.places[0];
        const uint16_t            knob = color_at(&q, st->x, st->y), well = color_at(&q, st->x + st->r * 3 / 4, st->y);
        CHECK(st->pictured && knob && well && knob != well);
        one(&q, st->x, st->y), one(&q, st->x + st->r, st->y);
        CHECK(q.axes[DGX_DI_AXIS_LX] == DGX_DI_AXIS_MAX && q.axes[DGX_DI_AXIS_LY] == 0);
        CHECK(color_at(&q, st->x + st->r / 2, st->y) == lighter_of(knob) && color_at(&q, st->x - st->r / 4, st->y) == well);
        none(&q);
        CHECK(color_at(&q, st->x, st->y) == knob && color_at(&q, st->x + st->r / 2 + st->r / 4, st->y) == well);
        dgx_di_pad_free(&q, NULL);
    }
    /* twin's A and B close (their circles apart, what they draw not): A pressed and let go, B whole where A's box took it */
    {
        dgx_di_pad_t q;
        CHECK(dgx_di_pad_open(&q, display, MM_W, MM_H, dgx_di_pad_layout_twin, dgx_di_pad_layout_twin_size));
        const dgx_di_pad_place_t *qa = &q.places[1], *qb = &q.places[2];
        int                       bx = -1, by = -1;
        for (int yy = qb->y - qb->r; yy <= qb->y + qb->r && bx < 0; ++yy)
            for (int xx = qb->x - qb->r; xx <= qb->x + qb->r && bx < 0; ++xx)
                if ((xx - qb->x) * (xx - qb->x) + (yy - qb->y) * (yy - qb->y) < (qb->r - 3) * (qb->r - 3) &&
                    (xx - qa->x) * (xx - qa->x) + (yy - qa->y) * (yy - qa->y) > (qa->r + 3) * (qa->r + 3) && xx >= qa->box[0] && xx <= qa->box[2] &&
                    yy >= qa->box[1] && yy <= qa->box[3])
                    bx = xx, by = yy;
        CHECK(bx >= 0);
        const unsigned b_was = shown(&q, bx, by);
        CHECK(b_was != 0);
        one(&q, qa->x, qa->y);
        CHECK(q.buttons == DGX_DI_PAD_A && shown(&q, bx, by) == b_was);
        none(&q);
        CHECK(shown(&q, bx, by) == b_was);
        dgx_di_pad_free(&q, NULL);
    }
    /* a button of a ring, a circle when pressed: let go, the circle gone (what it drew taken away first), the ring left */
    {
        static uint8_t           bb[256];
        static const hand_shape_t ring[] = {{0, 1, 1, 70, 10}, {1, 0, 2, 70, 0}};
        dgx_di_pad_t              q;
        CHECK(dgx_di_pad_open(&q, display, MM_W, MM_H, bb, blob_of(bb, DGX_DI_PAD_BUTTON, 70, 0x2E65, ring, 2)));
        const dgx_di_pad_place_t *it = &q.places[0];
        CHECK(shown(&q, it->x, it->y) == 0 && color_at(&q, it->x + it->r - 1, it->y) == 0x2E65);
        one(&q, it->x, it->y);
        CHECK(q.buttons == DGX_DI_PAD_A && color_at(&q, it->x, it->y) == lighter_of(0x2E65));
        none(&q);
        CHECK(shown(&q, it->x, it->y) == 0 && color_at(&q, it->x + it->r - 1, it->y) == 0x2E65);
        dgx_di_pad_free(&q, NULL);
    }
    /* a stick of a well of a ring and a knob larger than half of it: led to the rim, the knob past the well; let go, gone from there */
    {
        static uint8_t           bb[256];
        static const hand_shape_t big[] = {{0, 1, 1, 150, 10}, {1, 0, 1, 120, 0}, {2, 0, 2, 120, 0}};
        dgx_di_pad_t              q;
        CHECK(dgx_di_pad_open(&q, display, MM_W, MM_H, bb, blob_of(bb, DGX_DI_PAD_STICK, 150, 0x249D, big, 3)));
        const dgx_di_pad_place_t *it = &q.places[0];
        const int                 past = it->x + it->r * 115 / 100;
        one(&q, it->x, it->y), one(&q, it->x + it->r, it->y);
        CHECK(color_at(&q, past, it->y) == lighter_of(0x249D));
        none(&q);
        CHECK(shown(&q, past, it->y) == 0 && color_at(&q, it->x, it->y) == 0x249D);
        dgx_di_pad_free(&q, NULL);
    }
    /* blobs not of the format: refused */
    {
        static uint8_t bb[2048];
        dgx_di_pad_t   q;
        const size_t   n = dgx_di_pad_layout_flat_size, shapes = 16 + 56 * 7;
        memcpy(bb, dgx_di_pad_layout_flat, n);
        CHECK(dgx_di_pad_open(&q, display, MM_W, MM_H, bb, n));
        dgx_di_pad_free(&q, NULL);
        CHECK(!dgx_di_pad_open(&q, display, MM_W, MM_H, bb, 15));    /* short of its header */
        CHECK(!dgx_di_pad_open(&q, display, MM_W, MM_H, bb, n - 1)); /* its name not ended */
        CHECK(!dgx_di_pad_open(&q, display, MM_W, MM_H, NULL, n) && !dgx_di_pad_open(&q, display, MM_W, MM_H, bb, 300));
        static const struct {
            size_t  at;
            uint8_t to;
        } spoilt[] = {
            {0, 'X'},                  /* not DGXP */
            {4, 2},                    /* another version */
            {5, 0},                    /* no items */
            {5, 125},                  /* too many */
            {16, 3},                   /* an item of no kind */
            {17, 3},                   /* from nowhere */
            {16 + 8, 0},               /* a radius of none (its low byte; the high one is 0) */
            {16 + 56 + 16 + 8 + 2, 1}, /* a button's third picture: it has two */
            {shapes, 5},               /* a shape of no kind */
            {shapes + 1, 4},           /* painted by nothing */
        };
        for (size_t i = 0; i < sizeof spoilt / sizeof *spoilt; ++i) {
            memcpy(bb, dgx_di_pad_layout_flat, n);
            bb[spoilt[i].at] = spoilt[i].to;
            if (dgx_di_pad_open(&q, display, MM_W, MM_H, bb, n)) fprintf(stderr, "spoilt at %zu: opened\n", spoilt[i].at), dgx_di_pad_free(&q, NULL);
            else CHECK(!q.owned && !q.palette);
        }
        int opened = 0;
        for (size_t i = 0; i < sizeof spoilt / sizeof *spoilt; ++i) {
            memcpy(bb, dgx_di_pad_layout_flat, n);
            bb[spoilt[i].at] = spoilt[i].to;
            if (dgx_di_pad_open(&q, display, MM_W, MM_H, bb, n)) ++opened, dgx_di_pad_free(&q, NULL);
        }
        CHECK(opened == 0);
    }

    /* laid over the display and taken away, another pad not shown neither hiding nor freeing it; too many items: none */
    CHECK(dgx_di_pad_show(&pad, display, 160));
    {
        dgx_di_pad_t q;
        CHECK(dgx_di_pad_init(&q, display, MM_W, MM_H, dgx_di_pad_classic, DGX_DI_PAD_CLASSIC));
        dgx_di_pad_hide(&q, display);
        CHECK(pad.layer->overlay);
        dgx_di_pad_free(&q, display);
        CHECK(pad.layer->overlay);
    }
    dgx_di_pad_hide(&pad, display);
    CHECK(!pad.layer->overlay);
    {
        dgx_di_pad_t q;
        CHECK(!dgx_di_pad_init(&q, display, MM_W, MM_H, dgx_di_pad_classic, 200));
        CHECK(!dgx_di_pad_init(&q, display, 0, MM_H, dgx_di_pad_classic, DGX_DI_PAD_CLASSIC));
    }
    dgx_di_pad_free(&pad, display);
    dgx_screen_destroy(&display);
    CHECK_DONE();
}
