/*
 * A pad on the display: its items placed from millimetres, its mask and layer; a finger down on an item the pad's
 * (claimed: not the program's first finger), a button held within half its radius again, a cross as it is drawn
 * (none at the middle, an arm its way, a corner between two arms both), a stick by how far (fixed, floating); several fingers;
 * an item pressed drawn so, the others not; laid over the display and taken away. On a display of 1280x800 over
 * 217x136 mm, the 10" Guition's.
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

/* the layer's color index at x, y */
static unsigned shown(const dgx_di_pad_t *pad, int x, int y)
{
    return (unsigned)pad->layer->get_pixel(pad->layer, x, y);
}

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
    CHECK(shown(&pad, a->x, a->y) == a_color + 1 && shown(&pad, b->x, b->y) == b_color);
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
        CHECK(shown(&pad, cross->x + r * 9 / 10, cross->y) != shown(&pad, cross->x - r * 9 / 10, cross->y)); /* the arm held lighter */
        none(&pad);
        CHECK(pad.buttons == 0 && shown(&pad, cross->x + r * 9 / 10, cross->y) == shown(&pad, cross->x - r * 9 / 10, cross->y));
    }

    /* a pad of four ways only: no two at once */
    {
        static const dgx_di_pad_item_t four[] = {{DGX_DI_PAD_CROSS, DGX_DI_FROM_MIDDLE, DGX_DI_FROM_MIDDLE, DGX_DI_PAD_FOUR, 0, 0, 200, 0, 0}};
        dgx_di_pad_t                   q;
        CHECK(dgx_di_pad_init(&q, display, MM_W, MM_H, four, 1));
        const dgx_di_pad_place_t *c = &q.places[0];
        one(&q, c->x, c->y), one(&q, c->x + c->r * 6 / 10, c->y + c->r * 7 / 10);
        CHECK(q.buttons == DGX_DI_PAD_DOWN);
        none(&q);
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

    /* laid over the display and taken away; too many items: none */
    CHECK(dgx_di_pad_show(&pad, display, 160));
    dgx_di_pad_hide(&pad, display);
    {
        dgx_di_pad_t q;
        CHECK(!dgx_di_pad_init(&q, display, MM_W, MM_H, dgx_di_pad_classic, 200));
        CHECK(!dgx_di_pad_init(&q, display, 0, MM_H, dgx_di_pad_classic, DGX_DI_PAD_CLASSIC));
    }
    dgx_di_pad_free(&pad, display);
    dgx_screen_destroy(&display);
    CHECK_DONE();
}
