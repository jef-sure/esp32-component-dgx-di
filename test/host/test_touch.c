/*
 * The gestures of a touch over points of the display: pressed and released; a swipe from an edge, as the engine told
 * it before it moved here; a plain swipe by the way it went; a long press; one gesture a touch, an edge before a plain
 * swipe. In millimetres: a display of 800x480 over 80x48 mm, 10 pixels a millimetre — the band of an edge 90 pixels,
 * a swipe 150, the slop of a long press 30; and one denser across than down, and turned.
 */
#include <stdio.h>
#include <stdlib.h>

#include "check.h"
#include "dgx_di.h"

static const dgx_screen_t wide = {.width = 800, .height = 480, .dir_x = DgxScreenLeftRight, .dir_y = DgxScreenTopBottom};

/* down at x, y, then to x2, y2 a moment later: what came of it then */
static dgx_di_touch_t *stroke(dgx_di_touch_t *t, int x, int y, int x2, int y2)
{
    dgx_di_touch_take(t, x, y, true, 0);
    dgx_di_touch_take(t, x2, y2, true, 10);
    return t;
}

static void lift(dgx_di_touch_t *t)
{
    dgx_di_touch_take(t, t->x, t->y, false, 20);
}

int main(void)
{
    CHECK(sizeof(void *) > 4 || sizeof(dgx_di_touch_t) <= 32); /* 32 bytes on a 32-bit chip; a host's pointers are wider */
    /* down, up: pressed and released once; no display: no gestures */
    {
        dgx_di_touch_t t = {0};
        dgx_di_touch_take(&t, 10, 20, true, 0);
        CHECK(t.down && t.pressed && !t.released && t.x == 10 && t.y == 20);
        dgx_di_touch_take(&t, 11, 21, true, 10);
        CHECK(t.down && !t.pressed && !t.released);
        dgx_di_touch_take(&t, 11, 21, false, 20);
        CHECK(!t.down && !t.pressed && t.released);
        dgx_di_touch_take(&t, 11, 21, false, 30);
        CHECK(!t.released);
        dgx_di_touch_take(&t, 100, 5, true, 0), dgx_di_touch_take(&t, 100, 400, true, 2000);
        CHECK(!t.edge && !t.swipe && !t.long_press);
        dgx_di_touch_take(&t, 100, 400, false, 2010);
        dgx_di_touch_display(&t, &wide, 0, 48); /* no size: none either */
        dgx_di_touch_take(&t, 100, 5, true, 0), dgx_di_touch_take(&t, 100, 400, true, 2000);
        CHECK(!t.edge && !t.swipe && !t.long_press);
    }
    /* a swipe from an edge: down within 9 mm of it, away 15: the edge told once; else none */
    {
        dgx_di_touch_t e = {0};
        dgx_di_touch_display(&e, &wide, 80, 48);
        dgx_di_touch_take(&e, 300, 50, true, 0);
        dgx_di_touch_take(&e, 300, 199, true, 10);
        CHECK(e.edge == 0 && e.swipe == 0); /* 149 down: not yet */
        dgx_di_touch_take(&e, 302, 200, true, 20);
        CHECK(e.edge == DGX_DI_EDGE_TOP && e.swipe == 0 && e.down);
        dgx_di_touch_take(&e, 302, 400, true, 30);
        CHECK(e.edge == 0 && e.swipe == 0); /* told once, no plain swipe after it */
        lift(&e);
        CHECK(stroke(&e, 300, 90, 300, 300)->edge == 0 && e.swipe == DGX_DI_SWIPE_DOWN); /* 90 down: not in the band */
        lift(&e);
        CHECK(stroke(&e, 300, 89, 300, 300)->edge == DGX_DI_EDGE_TOP); /* 89: in it */
        lift(&e);
        CHECK(stroke(&e, 795, 240, 640, 240)->edge == DGX_DI_EDGE_RIGHT);
        lift(&e);
        CHECK(stroke(&e, 3, 475, 3, 320)->edge == DGX_DI_EDGE_BOTTOM); /* a corner: the way it went */
        lift(&e);
        CHECK(stroke(&e, 3, 475, 160, 475)->edge == DGX_DI_EDGE_LEFT);
        lift(&e);
        dgx_di_touch_take(&e, 300, 5, true, 0), dgx_di_touch_take(&e, 300, 5, false, 10), dgx_di_touch_take(&e, 300, 200, true, 20);
        CHECK(e.edge == 0); /* let go between: a new touch, not near the edge */
        lift(&e);
        /* the screen turned: its edges as it stands */
        static const dgx_screen_t tall = {.width = 480, .height = 800, .dir_x = DgxScreenRightLeft, .dir_y = DgxScreenTopBottom, .swap_xy = true};
        dgx_di_touch_display(&e, &tall, 48, 80);
        CHECK(stroke(&e, 475, 400, 320, 400)->edge == DGX_DI_EDGE_RIGHT);
        lift(&e);
    }
    /* a plain swipe: away 15 mm along an axis, the way it went, once a touch */
    {
        dgx_di_touch_t s = {0};
        dgx_di_touch_display(&s, &wide, 80, 48);
        dgx_di_touch_take(&s, 400, 240, true, 0), dgx_di_touch_take(&s, 549, 240, true, 10);
        CHECK(s.swipe == 0); /* 149 across: not yet */
        dgx_di_touch_take(&s, 550, 245, true, 20);
        CHECK(s.swipe == DGX_DI_SWIPE_RIGHT && s.edge == 0);
        dgx_di_touch_take(&s, 700, 245, true, 30);
        CHECK(s.swipe == 0); /* told once */
        dgx_di_touch_take(&s, 400, 420, true, 40);
        CHECK(s.swipe == 0); /* nor another way in the same touch */
        lift(&s);
        CHECK(stroke(&s, 400, 240, 250, 240)->swipe == DGX_DI_SWIPE_LEFT);
        lift(&s);
        CHECK(stroke(&s, 400, 240, 400, 90)->swipe == DGX_DI_SWIPE_UP);
        lift(&s);
        CHECK(stroke(&s, 400, 240, 400, 390)->swipe == DGX_DI_SWIPE_DOWN);
        lift(&s);
        /* both axes far enough at once: the one gone farther */
        CHECK(stroke(&s, 400, 240, 580, 410)->swipe == DGX_DI_SWIPE_RIGHT);
        lift(&s);
        CHECK(stroke(&s, 400, 240, 560, 420)->swipe == DGX_DI_SWIPE_DOWN);
        lift(&s);
        CHECK(stroke(&s, 400, 240, 570, 420)->swipe == DGX_DI_SWIPE_DOWN); /* 170 against 180 */
        lift(&s);
        CHECK(stroke(&s, 400, 240, 560, 400)->swipe == DGX_DI_SWIPE_DOWN); /* as far each way: along y */
        lift(&s);
        /* down at the top edge, gone across first: a plain swipe, and then no edge in that touch */
        CHECK(stroke(&s, 400, 5, 560, 10)->swipe == DGX_DI_SWIPE_RIGHT && s.edge == 0);
        dgx_di_touch_take(&s, 560, 200, true, 20);
        CHECK(s.edge == 0 && s.swipe == 0);
        lift(&s);
        /* down at the top edge, gone down: the edge, not a swipe down */
        CHECK(stroke(&s, 400, 5, 400, 160)->edge == DGX_DI_EDGE_TOP && s.swipe == 0);
        lift(&s);
    }
    /* a long press: down 500 ms within 3 mm of where it went down; once a touch, no swipe after it */
    {
        dgx_di_touch_t l = {0};
        dgx_di_touch_display(&l, &wide, 80, 48);
        dgx_di_touch_take(&l, 400, 240, true, 1000);
        dgx_di_touch_take(&l, 402, 242, true, 1499);
        CHECK(!l.long_press);
        dgx_di_touch_take(&l, 430, 270, true, 1500); /* 30 off: within */
        CHECK(l.long_press && !l.swipe);
        dgx_di_touch_take(&l, 430, 270, true, 1600);
        CHECK(!l.long_press); /* told once */
        dgx_di_touch_take(&l, 700, 270, true, 1700);
        CHECK(!l.swipe); /* the touch has told its gesture */
        lift(&l);
        /* gone farther than the slop for a moment: none, though it came back */
        dgx_di_touch_take(&l, 400, 240, true, 0);
        dgx_di_touch_take(&l, 431, 240, true, 100);
        dgx_di_touch_take(&l, 400, 240, true, 200);
        dgx_di_touch_take(&l, 400, 240, true, 900);
        CHECK(!l.long_press);
        lift(&l);
        /* by an edge: a rest there is no long press, the swipe from the edge after it is told */
        dgx_di_touch_take(&l, 400, 5, true, 0);
        dgx_di_touch_take(&l, 402, 8, true, 900);
        CHECK(!l.long_press);
        dgx_di_touch_take(&l, 402, 160, true, 1000);
        CHECK(l.edge == DGX_DI_EDGE_TOP);
        lift(&l);
        /* a swipe, then held: none */
        dgx_di_touch_take(&l, 400, 240, true, 0);
        dgx_di_touch_take(&l, 550, 240, true, 100);
        CHECK(l.swipe == DGX_DI_SWIPE_RIGHT);
        dgx_di_touch_take(&l, 550, 240, true, 900);
        CHECK(!l.long_press);
        lift(&l);
        /* let go before 500 ms: none; the clock past its 32 bits: running on */
        dgx_di_touch_take(&l, 400, 240, true, 0), dgx_di_touch_take(&l, 400, 240, false, 499);
        dgx_di_touch_take(&l, 400, 240, true, 500);
        CHECK(!l.long_press && l.pressed);
        lift(&l);
        dgx_di_touch_take(&l, 400, 240, true, 0xffffff00u), dgx_di_touch_take(&l, 400, 240, true, 300);
        CHECK(l.long_press); /* 556 ms later */
    }
    /*
     * 800x480 over 80x96 mm: 10 a millimetre across, 5 down — a band of 45 at the top, a swipe of 75 down, a slop of 15
     * down; then the screen turned: the millimetres with it
     */
    {
        dgx_screen_t   turning = wide;
        dgx_di_touch_t d       = {0};
        dgx_di_touch_display(&d, &turning, 80, 96);
        CHECK(stroke(&d, 400, 29, 400, 104)->edge == DGX_DI_EDGE_TOP);
        lift(&d);
        CHECK(stroke(&d, 400, 45, 400, 120)->edge == 0 && d.swipe == DGX_DI_SWIPE_DOWN); /* 45 down: not in the band */
        lift(&d);
        CHECK(stroke(&d, 400, 240, 400, 314)->swipe == 0); /* 74 down: not yet */
        lift(&d);
        /* 160 across is 16 mm, 90 down 18: the larger share down, though fewer pixels */
        CHECK(stroke(&d, 400, 240, 560, 330)->swipe == DGX_DI_SWIPE_DOWN);
        lift(&d);
        dgx_di_touch_take(&d, 400, 240, true, 0), dgx_di_touch_take(&d, 400, 256, true, 100), dgx_di_touch_take(&d, 400, 240, true, 600);
        CHECK(!d.long_press); /* 16 down is past 3 mm */
        lift(&d);
        turning = (dgx_screen_t){.width = 480, .height = 800, .dir_x = DgxScreenRightLeft, .dir_y = DgxScreenTopBottom, .swap_xy = true};
        CHECK(stroke(&d, 440, 400, 365, 400)->edge == DGX_DI_EDGE_RIGHT); /* 5 a millimetre across now: a band of 45, 75 to go */
        lift(&d);
        CHECK(stroke(&d, 430, 400, 300, 400)->edge == 0 && d.swipe == DGX_DI_SWIPE_LEFT); /* 50 from the edge: not in it */
        lift(&d);
        CHECK(stroke(&d, 240, 400, 240, 550)->swipe == DGX_DI_SWIPE_DOWN && stroke(&d, 240, 400, 240, 549)->swipe == 0);
        lift(&d);
    }
    CHECK_DONE();
}
