/*
 * Rectangles over the fingers of a pointer: a finger is the rectangle's it went down in, until it goes up — pressed,
 * held, inside or led out, let go inside (a click) or out; a long press, the click it takes away; a finger down beside
 * all of them holds none, nor does one claimed; several fingers, of several rectangles and of one; overlapping, the
 * first; the array taken again.
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "check.h"
#include "dgx_di.h"

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

static fake_t           f = {.device = {.read = fake_read, .fingers = 4, .x_min = 0, .x_max = 99, .y_min = 0, .y_max = 99}};
static dgx_screen_t     s = {.width = 100, .height = 100, .dir_x = DgxScreenLeftRight, .dir_y = DgxScreenTopBottom};
static dgx_di_pointer_t p;
static uint32_t         now;

/* fingers at these places (id, x, y), or none; a read 10 ms later; the rectangles taken */
static int look(dgx_di_rects_t *r, int n, const int (*at)[3])
{
    f.n = n;
    for (int i = 0; i < n; ++i) f.points[i] = (dgx_di_point_t){.x = (int16_t)at[i][1], .y = (int16_t)at[i][2], .id = (uint8_t)at[i][0]};
    now += 10;
    dgx_di_pointer_read(&p, now);
    return dgx_di_rects_take(r, &p, now);
}

static int one(dgx_di_rects_t *r, int x, int y)
{
    const int at[1][3] = {{1, x, y}};
    return look(r, 1, at);
}

static int none(dgx_di_rects_t *r)
{
    return look(r, 0, NULL);
}

int main(void)
{
    dgx_di_pointer_init(&p, &f.device, 0, &s);
    /* a row of three, and one over the third (the first of them wins) */
    dgx_di_rect_t  b[4] = {{10, 10, 20, 20, 0, 0}, {40, 10, 20, 20, 0, 0}, {70, 10, 20, 20, 0, 0}, {65, 5, 30, 30, 0, 0}};
    dgx_di_rects_t r;
    dgx_di_rects_init(&r, b, 4);

    /* down in the second, held, let go inside: pressed, held and inside, then a click */
    CHECK(one(&r, 45, 15) == 1);
    CHECK(b[1].flags == (DGX_DI_RECT_PRESSED | DGX_DI_RECT_HELD | DGX_DI_RECT_INSIDE) && b[1].fingers == 1 && b[0].flags == 0);
    CHECK(one(&r, 47, 17) == -1 && b[1].flags == (DGX_DI_RECT_HELD | DGX_DI_RECT_INSIDE));
    CHECK(none(&r) == -1 && b[1].flags == (DGX_DI_RECT_RELEASED | DGX_DI_RECT_CLICKED) && b[1].fingers == 0);
    CHECK(none(&r) == -1 && b[1].flags == 0);

    /* led out over the first and let go there: held but not inside, no press of the first, no click */
    CHECK(one(&r, 45, 15) == 1);
    CHECK(one(&r, 15, 15) == -1 && b[1].flags == DGX_DI_RECT_HELD && b[0].flags == 0);
    CHECK(none(&r) == -1 && b[1].flags == DGX_DI_RECT_RELEASED && b[0].flags == 0);
    /* led out and back: a click */
    one(&r, 45, 15), one(&r, 80, 80), one(&r, 50, 20);
    CHECK(b[1].flags == (DGX_DI_RECT_HELD | DGX_DI_RECT_INSIDE));
    CHECK(none(&r) == -1 && b[1].flags == (DGX_DI_RECT_RELEASED | DGX_DI_RECT_CLICKED));

    /* its border: the first pixels in, the next ones out */
    static const int border[][3] = {{10, 10, 0}, {29, 29, 0}, {30, 15, -1}, {15, 30, -1}, {9, 15, -1}, {15, 9, -1}};
    for (size_t i = 0; i < sizeof border / sizeof *border; ++i) {
        if (one(&r, border[i][0], border[i][1]) != border[i][2]) fprintf(stderr, "at %d, %d: not %d\n", border[i][0], border[i][1], border[i][2]);
        CHECK(r.of[0] == border[i][2] || p.fingers[0].pressed == false);
        none(&r);
    }
    CHECK(one(&r, 10, 10) == 0);
    none(&r);
    CHECK(one(&r, 30, 15) == -1);
    none(&r);

    /* down beside all of them: none; over one after: still none */
    CHECK(one(&r, 50, 80) == -1);
    CHECK(one(&r, 45, 15) == -1 && b[1].flags == 0);
    none(&r);

    /* the third and the one over it: the first of the array */
    CHECK(one(&r, 75, 15) == 2 && b[2].flags & DGX_DI_RECT_PRESSED && b[3].flags == 0);
    CHECK(none(&r) == -1 && b[2].flags & DGX_DI_RECT_CLICKED);
    CHECK(one(&r, 67, 7) == 3 && b[3].flags & DGX_DI_RECT_PRESSED); /* only the one over it there */
    none(&r);

    /* a long press: 500 ms inside, told once; let go after it, no click */
    CHECK(one(&r, 15, 15) == 0);
    bool told = false, twice = false;
    for (int i = 0; i < 60; ++i) {
        one(&r, 16, 16);
        if (b[0].flags & DGX_DI_RECT_LONG) twice |= told, told = true;
    }
    CHECK(told && !twice);
    CHECK(none(&r) == -1 && b[0].flags == DGX_DI_RECT_RELEASED);
    /* out a moment before 500 ms: none, the click still */
    one(&r, 15, 15), one(&r, 35, 15), one(&r, 15, 15);
    for (int i = 0; i < 60; ++i) {
        one(&r, 15, 15);
        CHECK(!(b[0].flags & DGX_DI_RECT_LONG));
    }
    CHECK(none(&r) == -1 && b[0].flags == (DGX_DI_RECT_RELEASED | DGX_DI_RECT_CLICKED));

    /* two fingers in two: both pressed, the first of the fingers told; one up: its click, the other held */
    {
        const int two[2][3] = {{1, 15, 15}, {2, 45, 15}}, other_way[2][3] = {{2, 45, 15}, {1, 15, 15}};
        CHECK(look(&r, 2, other_way) == 0 && b[0].flags & DGX_DI_RECT_PRESSED && b[1].flags & DGX_DI_RECT_PRESSED); /* the first in the array */
        none(&r);
        CHECK(look(&r, 2, two) == 0);
        const int left[1][3] = {{2, 45, 15}};
        look(&r, 1, left);
        CHECK(b[0].flags == (DGX_DI_RECT_RELEASED | DGX_DI_RECT_CLICKED) && b[1].flags == (DGX_DI_RECT_HELD | DGX_DI_RECT_INSIDE));
        none(&r);
    }
    /* two fingers in one: two of it; one up: a click and still held by the other */
    {
        const int two[2][3] = {{1, 12, 12}, {2, 25, 25}};
        look(&r, 2, two);
        CHECK(b[0].fingers == 2 && b[0].flags & DGX_DI_RECT_PRESSED);
        const int left[1][3] = {{2, 25, 25}};
        look(&r, 1, left);
        CHECK(b[0].fingers == 1 && b[0].flags == (DGX_DI_RECT_HELD | DGX_DI_RECT_INSIDE | DGX_DI_RECT_RELEASED | DGX_DI_RECT_CLICKED));
        none(&r);
    }
    /* a finger claimed at the read it went down at (a pad's, taken before the rectangles): none's, though down in one;
       another, not claimed, its own */
    {
        f.n = 1, f.points[0] = (dgx_di_point_t){.x = 15, .y = 15, .id = 1};
        dgx_di_pointer_read(&p, now += 10);
        for (int i = 0; i < DGX_DI_FINGERS; ++i)
            if (p.fingers[i].pressed) dgx_di_pointer_claim(&p, i);
        CHECK(dgx_di_rects_take(&r, &p, now) == -1 && b[0].flags == 0 && b[0].fingers == 0);
        const int two[2][3] = {{1, 16, 16}, {2, 45, 15}};
        CHECK(look(&r, 2, two) == 1 && b[0].flags == 0 && b[1].flags == (DGX_DI_RECT_PRESSED | DGX_DI_RECT_HELD | DGX_DI_RECT_INSIDE));
        CHECK(none(&r) == -1 && b[0].flags == 0 && b[1].flags == (DGX_DI_RECT_RELEASED | DGX_DI_RECT_CLICKED));
    }
    /* the array taken again, smaller, a finger of a rectangle gone: it is none's */
    CHECK(one(&r, 75, 15) == 2);
    dgx_di_rects_init(&r, b, 2);
    CHECK(one(&r, 75, 15) == -1 && b[0].flags == 0 && b[1].flags == 0);
    CHECK(none(&r) == -1);
    /* no rectangles: nothing, and no harm */
    dgx_di_rects_init(&r, NULL, 3);
    CHECK(one(&r, 15, 15) == -1 && r.count == 0);
    none(&r);
    CHECK_DONE();
}
