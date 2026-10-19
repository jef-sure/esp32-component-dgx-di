/*
 * The glue of a pointer: a device's fingers as fingers of the display — mirrored, swapped, scaled, calibrated, turned as
 * the screen is turned now — and reads: down, where; up, where it went up; steadied by time; known by their numbers,
 * or by where they were when the device keeps none; the first finger. A panel of 800x1280 (turned to
 * 1280x800), a resistive panel's range and a smaller device under a larger display.
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "check.h"
#include "dgx_di.h"

/* a device of the test: what it sees now */
typedef struct {
    dgx_di_device_t device;
    int             n;
    dgx_di_point_t  points[DGX_DI_FINGERS + 3];
} fake_t;

static int fake_read(dgx_di_device_t *device, dgx_di_point_t *points, int max)
{
    const fake_t *f = (const fake_t *)device;
    const int     n = f->n < max ? f->n : max;
    memcpy(points, f->points, sizeof *points * (size_t)n);
    return n;
}

static fake_t fake(int x_min, int x_max, int y_min, int y_max)
{
    return (fake_t){.device = {.read = fake_read, .fingers = DGX_DI_FINGERS, .x_min = (int16_t)x_min, .x_max = (int16_t)x_max, .y_min = (int16_t)y_min,
                               .y_max = (int16_t)y_max}};
}

/* a screen of DGX as it stands: its size and orientation */
static dgx_screen_t screen(int width, int height, dgx_orientation_t dir_x, dgx_orientation_t dir_y, bool swap_xy)
{
    return (dgx_screen_t){.width = width, .height = height, .dir_x = dir_x, .dir_y = dir_y, .swap_xy = swap_xy};
}

static dgx_di_point_t point(int x, int y, int id)
{
    return (dgx_di_point_t){.x = (int16_t)x, .y = (int16_t)y, .id = (uint8_t)id};
}

static bool at(const dgx_di_pointer_t *p, int dx, int dy, int x, int y)
{
    int sx, sy;
    dgx_di_pointer_map(p, dx, dy, &sx, &sy, NULL);
    if (sx != x || sy != y) fprintf(stderr, "(%d, %d) -> (%d, %d), not (%d, %d)\n", dx, dy, sx, sy, x, y);
    return sx == x && sy == y;
}

/* a calibration of points alone, as one is written by hand, for the pointer's device and display */
static dgx_di_calibration_t knotted(const dgx_di_pointer_t *p, int flags, const dgx_di_knot_t *x, int nx, const dgx_di_knot_t *y, int ny)
{
    const dgx_screen_t  *s = p->screen;
    dgx_di_calibration_t c = {.version = DGX_DI_CALIBRATION_VERSION, .size = sizeof c, .x_min = p->device->x_min, .x_max = p->device->x_max,
                              .y_min = p->device->y_min, .y_max = p->device->y_max, .width = (int16_t)(s->swap_xy ? s->height : s->width),
                              .height = (int16_t)(s->swap_xy ? s->width : s->height), .flags = (uint8_t)flags, .knots_x = (uint8_t)nx,
                              .knots_y = (uint8_t)ny};
    if (nx) memcpy(c.x, x, sizeof *x * (size_t)nx);
    if (ny) memcpy(c.y, y, sizeof *y * (size_t)ny);
    return c;
}

/* the finger of that number, in use */
static const dgx_di_finger_t *finger(const dgx_di_pointer_t *p, int id)
{
    for (int i = 0; i < DGX_DI_FINGERS; ++i) {
        const dgx_di_finger_t *f = &p->fingers[i];
        if ((f->down || f->sees || f->released) && f->id == id) return f;
    }
    return NULL;
}

int main(void)
{
    const dgx_orientation_t LR = DgxScreenLeftRight, RL = DgxScreenRightLeft, TB = DgxScreenTopBottom, BT = DgxScreenBottomTop;
    dgx_di_pointer_t        p;

    /*
     * a panel of 800 across and 1280 along, lying as the display does in its own orientation (no flags); the
     * screen turned to 1280x800 by DGX (counted the other way along both axes of the panel, the axes swapped): where a
     * pointer of 0.0.1 had all three flags against a display of 1280x800
     */
    {
        fake_t       f = fake(0, 799, 0, 1279);
        dgx_screen_t s = screen(1280, 800, RL, BT, true);
        dgx_di_pointer_init(&p, &f.device, 0, &s);
        CHECK(at(&p, 0, 0, 1279, 799));
        CHECK(at(&p, 799, 1279, 0, 0));
        CHECK(at(&p, 799, 0, 1279, 0));
        CHECK(at(&p, 0, 1279, 0, 799));
        CHECK(at(&p, 100, 200, 1279 - 200, 799 - 100));
        CHECK(at(&p, -5, 5000, 0, 799)); /* past its range: at its edge */
        /* the screen turned the other way round: the fingers with it */
        s = screen(1280, 800, LR, TB, true);
        CHECK(at(&p, 0, 0, 0, 0));
        CHECK(at(&p, 100, 200, 200, 100));
        /* upright, and upside down */
        s = screen(800, 1280, LR, TB, false);
        CHECK(at(&p, 100, 200, 100, 200));
        s = screen(800, 1280, RL, BT, false);
        CHECK(at(&p, 100, 200, 699, 1079));
    }

    /*
     * the 10" of dgx-ge-prince: its GSL3680 (1664x896, x along) under the JD9365 (800x1280) turned against the clock
     * (RightLeft, TopBottom, swapped). With SWAP_XY | MIRROR_Y and its calibration moved to the panel's own x (799 less
     * the screen's y), what 0.0.1 gave it with no flags against 1280x800 and the calibration of the screen's y — the same
     * everywhere within a pixel (halves rounded away from 0 turn round with the axis)
     */
    {
        static const dgx_di_knot_t now_x[] = {{20, 48}, {399, 399}, {747, 751}}, then_y[] = {{52, 48}, {400, 400}, {779, 751}};
        fake_t                     f   = fake(0, 1663, 0, 895);
        dgx_screen_t               s   = screen(1280, 800, RL, TB, true), then = screen(1280, 800, LR, TB, false);
        dgx_di_pointer_t           old = {0};
        dgx_di_pointer_init(&p, &f.device, 0, &s);
        const dgx_di_calibration_t now = knotted(&p, DGX_DI_SWAP_XY | DGX_DI_MIRROR_Y, now_x, 3, NULL, 0);
        CHECK(dgx_di_pointer_calibrate(&p, &now) && p.flags == (DGX_DI_SWAP_XY | DGX_DI_MIRROR_Y));
        dgx_di_pointer_init(&old, &f.device, 0, &then);
        const dgx_di_calibration_t was = knotted(&old, 0, NULL, 0, then_y, 3);
        CHECK(dgx_di_pointer_calibrate(&old, &was));
        int worst = 0, exact = 0, all = 0;
        for (int dx = 0; dx < 1664; dx += 3) {
            for (int dy = 0; dy < 896; dy += 2) {
                int x, y, ox, oy;
                dgx_di_pointer_map(&p, dx, dy, &x, &y, NULL);
                dgx_di_pointer_map(&old, dx, dy, &ox, &oy, NULL);
                const int off = abs(x - ox) > abs(y - oy) ? abs(x - ox) : abs(y - oy);
                worst = off > worst ? off : worst, exact += !off, ++all;
            }
        }
        if (worst > 1 || exact < all * 99 / 100) fprintf(stderr, "the 10\": %d of %d the same, %d off at worst\n", exact, all, worst);
        CHECK(worst <= 1 && exact >= all * 99 / 100);
    }

    /* each flag alone */
    {
        fake_t       f = fake(0, 99, 0, 49);
        dgx_screen_t s = screen(100, 50, LR, TB, false);
        dgx_di_pointer_init(&p, &f.device, 0, &s);
        CHECK(at(&p, 10, 20, 10, 20));
        dgx_di_pointer_init(&p, &f.device, DGX_DI_MIRROR_X, &s);
        CHECK(at(&p, 10, 20, 89, 20));
        dgx_di_pointer_init(&p, &f.device, DGX_DI_MIRROR_Y, &s);
        CHECK(at(&p, 10, 20, 10, 29));
        s = screen(50, 100, LR, TB, false);
        dgx_di_pointer_init(&p, &f.device, DGX_DI_SWAP_XY, &s);
        CHECK(at(&p, 10, 20, 20, 10));
        /* a GSL3680 of 1664x896, its x along, under a panel of 800x1280 upright: swapped */
        fake_t g = fake(0, 1663, 0, 895);
        s        = screen(800, 1280, LR, TB, false);
        dgx_di_pointer_init(&p, &g.device, DGX_DI_SWAP_XY, &s);
        CHECK(at(&p, 0, 0, 0, 0));
        CHECK(at(&p, 1663, 895, 799, 1279));
        CHECK(at(&p, 832, 448, 400, 640));
    }

    /* scaled: a range from 0 (a resistive panel's ADC) and one from where its ADC begins, over 320x240 */
    {
        dgx_screen_t s = screen(320, 240, LR, TB, false);
        fake_t       f = fake(0, 4095, 0, 4095);
        dgx_di_pointer_init(&p, &f.device, 0, &s);
        CHECK(at(&p, 0, 0, 0, 0));
        CHECK(at(&p, 4095, 4095, 319, 239));
        CHECK(at(&p, 2048, 2048, 160, 120));
        f = fake(128, 3967, 128, 3967);
        dgx_di_pointer_init(&p, &f.device, 0, &s);
        CHECK(at(&p, 128, 128, 0, 0));
        CHECK(at(&p, 3967, 3967, 319, 239));
        CHECK(at(&p, 2048, 2048, 160, 120));
        CHECK(at(&p, 0, 4095, 0, 239)); /* past its range */
        /* a smaller device over a larger display: its cells spread, the ends within */
        f = fake(0, 3, 0, 1);
        s = screen(8, 8, LR, TB, false);
        dgx_di_pointer_init(&p, &f.device, 0, &s);
        CHECK(at(&p, 0, 0, 1, 2));
        CHECK(at(&p, 3, 1, 7, 6));
        /* a range of nothing: one cell */
        f = fake(5, 4, 5, 4);
        dgx_di_pointer_init(&p, &f.device, 0, &s);
        CHECK(at(&p, 0, 0, 4, 4));
    }

    /*
     * calibrated: the 10"'s points (its bottom taken 30 lower); at the points exact, between in proportion, past the ends
     * as the end's piece goes, within the display; taken: before it
     */
    {
        static const dgx_di_knot_t kx[] = {{38, 48}, {655, 640}, {1246, 1231}}, ky[] = {{49, 48}, {395, 400}, {782, 751}};
        fake_t                     f = fake(0, 1279, 0, 799);
        dgx_screen_t               s = screen(1280, 800, LR, TB, false);
        dgx_di_pointer_init(&p, &f.device, 0, &s);
        const dgx_di_calibration_t both = knotted(&p, 0, kx, 3, ky, 3), one = knotted(&p, 0, kx, 1, NULL, 0);
        CHECK(dgx_di_pointer_calibrate(&p, &both));
        CHECK(at(&p, 38, 49, 48, 48));
        CHECK(at(&p, 655, 395, 640, 400));
        CHECK(at(&p, 1246, 782, 1231, 751));
        CHECK(at(&p, 346, 222, 344, 224));   /* halfway: (48 + 640) / 2, (48 + 400) / 2 */
        CHECK(at(&p, 1000, 600, 985, 586));  /* 640 + 345 * 591 / 591, 400 + 205 * 351 / 387 */
        CHECK(at(&p, 0, 0, 12, 0));          /* past the first: as its piece goes (-2 by y: within) */
        CHECK(at(&p, 1279, 799, 1264, 766)); /* past the last */
        int16_t taken[2];
        int     sx, sy;
        dgx_di_pointer_map(&p, 1246, 782, &sx, &sy, taken);
        CHECK(taken[0] == 1246 && taken[1] == 782 && sx == 1231 && sy == 751);
        /* one point: a shift */
        CHECK(dgx_di_pointer_calibrate(&p, &one));
        CHECK(at(&p, 100, 100, 110, 100));
        /* the screen turned round: the calibrated point turned with it, the calibration the panel's own */
        CHECK(dgx_di_pointer_calibrate(&p, &both));
        s = screen(1280, 800, RL, BT, false);
        CHECK(at(&p, 1246, 782, 1279 - 1231, 799 - 751));
        dgx_di_pointer_map(&p, 1246, 782, &sx, &sy, taken);
        CHECK(taken[0] == 1246 && taken[1] == 782);
        CHECK(dgx_di_pointer_calibrate(&p, NULL));
        CHECK(at(&p, 1246, 782, 1279 - 1246, 799 - 782));
        /* not for this pointer: another range, another display, another form; nothing changed */
        dgx_di_calibration_t other = both;
        other.x_max = 1663;
        CHECK(!dgx_di_pointer_calibrate(&p, &other) && p.calibration == NULL);
        other = both, other.width = 800, other.height = 1280;
        CHECK(!dgx_di_pointer_calibrate(&p, &other));
        other = both, other.version = 2;
        CHECK(!dgx_di_pointer_calibrate(&p, &other));
        other = both, other.size = 12;
        CHECK(!dgx_di_pointer_calibrate(&p, &other));
        other = both, other.knots_x = DGX_DI_KNOTS + 1;
        CHECK(!dgx_di_pointer_calibrate(&p, &other));
        /* the times it was steadied by, when it has them */
        dgx_di_pointer_debounce(&p, 5, 7);
        other = both, other.down_ms = 60, other.up_ms = 100;
        CHECK(dgx_di_pointer_calibrate(&p, &other) && p.down_ms == 60 && p.up_ms == 100);
        CHECK(dgx_di_pointer_calibrate(&p, &both) && p.down_ms == 60 && p.up_ms == 100);
        /* the affine: twice as far across about the left edge, 10 lower */
        other = both, other.knots_x = other.knots_y = 0, other.affine[0] = 2 * 65536, other.affine[4] = 65536, other.affine[5] = 10 * 65536;
        CHECK(dgx_di_pointer_calibrate(&p, &other));
        s = screen(1280, 800, LR, TB, false);
        CHECK(at(&p, 100, 100, 200, 110));
        CHECK(at(&p, 1000, 795, 1279, 799)); /* within the display */
        /* half a pixel: rounded up */
        other.affine[0] = 65536, other.affine[2] = 32768, other.affine[5] = -32768 - 1;
        CHECK(at(&p, 100, 100, 101, 99));
    }

    /* reads of a device that numbers its fingers: two at once, moved, one up; the first finger */
    {
        fake_t       f = fake(0, 99, 0, 99);
        dgx_screen_t s = screen(100, 100, LR, TB, false);
        int          x, y;
        dgx_di_pointer_init(&p, &f.device, 0, &s);
        CHECK(dgx_di_pointer_read(&p, 0) == 0 && !dgx_di_pointer_first(&p, &x, &y));
        f.n = 2, f.points[0] = point(10, 20, 3), f.points[1] = point(60, 70, 7), f.points[1].size = 5;
        CHECK(dgx_di_pointer_read(&p, 10) == 2);
        const dgx_di_finger_t *a = finger(&p, 3), *b = finger(&p, 7);
        CHECK(a && b && a->pressed && a->down && a->x == 10 && a->y == 20 && b->pressed && b->x == 60 && b->size == 5);
        CHECK(dgx_di_pointer_first(&p, &x, &y) && x == 10 && y == 20); /* both at once: the one in the first place */
        /* moved, the device telling them the other way round: known by the numbers */
        f.points[0] = point(61, 71, 7), f.points[1] = point(11, 21, 3);
        CHECK(dgx_di_pointer_read(&p, 20) == 2);
        CHECK(a->x == 11 && a->y == 21 && b->x == 61 && !a->pressed && !b->pressed);
        CHECK(dgx_di_pointer_first(&p, &x, &y) && x == 11 && y == 21);
        /* the first up, the other left down: none is first, the first's place stays */
        f.n = 1, f.points[0] = point(62, 72, 7);
        CHECK(dgx_di_pointer_read(&p, 30) == 1);
        CHECK(a->released && !a->down && a->x == 11 && b->down && b->x == 62);
        CHECK(!dgx_di_pointer_first(&p, &x, &y) && x == 11 && y == 21);
        /* a new finger beside the one left down: no first either */
        f.n = 2, f.points[1] = point(30, 30, 1);
        CHECK(dgx_di_pointer_read(&p, 40) == 2);
        CHECK(finger(&p, 3) == NULL && finger(&p, 1)->pressed);
        CHECK(!dgx_di_pointer_first(&p, &x, &y));
        /* all up, then one down: the first again */
        f.n = 0;
        CHECK(dgx_di_pointer_read(&p, 50) == 0 && finger(&p, 7)->released && finger(&p, 1)->released);
        f.n = 1, f.points[0] = point(40, 45, 3);
        CHECK(dgx_di_pointer_read(&p, 60) == 1);
        CHECK(dgx_di_pointer_first(&p, &x, &y) && x == 40 && y == 45);
        f.n = 0;
        CHECK(dgx_di_pointer_read(&p, 70) == 0 && !dgx_di_pointer_first(&p, &x, &y) && x == 40 && y == 45);
        /* more fingers than are followed: so many of them; as many as the device sees; one when it does not say */
        f.n = DGX_DI_FINGERS + 3;
        for (int i = 0; i < f.n; ++i) f.points[i] = point(5 * i, 5 * i, 20 + i);
        CHECK(dgx_di_pointer_read(&p, 80) == DGX_DI_FINGERS);
        f.n = 0, dgx_di_pointer_read(&p, 90), dgx_di_pointer_read(&p, 100);
        f.n = 5, f.device.fingers = 3;
        CHECK(dgx_di_pointer_read(&p, 110) == 3);
        f.n = 0, dgx_di_pointer_read(&p, 120), dgx_di_pointer_read(&p, 130);
        f.n = 5, f.device.fingers = 0;
        CHECK(dgx_di_pointer_read(&p, 140) == 1);
        /* no device, or nothing it reads */
        dgx_di_pointer_init(&p, NULL, 0, &s);
        CHECK(dgx_di_pointer_read(&p, 0) == 0);
    }

    /*
     * claimed fingers (a pad's): never first, and down they keep none from being first; the first may go down beside
     * one held, and a finger left down claimed is no reason to wait for all up
     */
    {
        fake_t       f = fake(0, 99, 0, 99);
        dgx_screen_t s = screen(100, 100, LR, TB, false);
        int          x, y;
        dgx_di_pointer_init(&p, &f.device, 0, &s);
        f.n = 1, f.points[0] = point(80, 80, 1);
        dgx_di_pointer_read(&p, 0);
        CHECK(dgx_di_pointer_first(&p, &x, &y) && x == 80); /* taken for the first at the read */
        dgx_di_pointer_claim(&p, 0);
        CHECK(!dgx_di_pointer_first(&p, &x, &y) && p.fingers[0].claimed); /* claimed after it: not first */
        f.n = 2, f.points[1] = point(20, 20, 2);
        dgx_di_pointer_read(&p, 10);
        CHECK(dgx_di_pointer_first(&p, &x, &y) && x == 20); /* beside the claimed one: first */
        f.n = 1, f.points[0] = point(80, 80, 1);
        dgx_di_pointer_read(&p, 20);
        CHECK(!dgx_di_pointer_first(&p, &x, &y) && x == 20); /* the first up, only the claimed one down: none first... */
        f.n = 2, f.points[1] = point(30, 30, 3);
        dgx_di_pointer_read(&p, 30);
        CHECK(dgx_di_pointer_first(&p, &x, &y) && x == 30); /* ...and the next one first at once */
        f.n = 0;
        dgx_di_pointer_read(&p, 40);
        f.n = 1, f.points[0] = point(50, 50, 4);
        dgx_di_pointer_read(&p, 50);
        CHECK(!p.fingers[0].claimed || p.fingers[0].id != 4); /* a new finger is free */
        CHECK(dgx_di_pointer_first(&p, &x, &y) && x == 50);
        /* two down at one read, the first of them claimed: the other first */
        f.n = 0, dgx_di_pointer_read(&p, 60);
        f.n = 2, f.points[0] = point(10, 10, 5), f.points[1] = point(60, 60, 6);
        dgx_di_pointer_read(&p, 70);
        const int first_index = p.first;
        CHECK(first_index >= 0 && p.fingers[first_index].x == 10);
        dgx_di_pointer_claim(&p, first_index);
        CHECK(dgx_di_pointer_first(&p, &x, &y) && x == 60);
        dgx_di_pointer_claim(&p, -1), dgx_di_pointer_claim(&p, DGX_DI_FINGERS); /* no such finger: nothing */
    }

    /* a device that keeps no numbers: each finger taken for the nearest of the last read */
    {
        fake_t       f = fake(0, 99, 0, 99);
        dgx_screen_t s = screen(100, 100, LR, TB, false);
        dgx_di_pointer_init(&p, &f.device, 0, &s);
        f.n = 2, f.points[0] = point(10, 10, DGX_DI_NO_ID), f.points[1] = point(90, 90, DGX_DI_NO_ID);
        CHECK(dgx_di_pointer_read(&p, 0) == 2);
        const dgx_di_finger_t *a = finger(&p, 0), *b = finger(&p, 1);
        CHECK(a && b && a->x == 10 && b->x == 90);
        f.points[0] = point(88, 88, DGX_DI_NO_ID), f.points[1] = point(12, 12, DGX_DI_NO_ID);
        CHECK(dgx_di_pointer_read(&p, 10) == 2 && a->x == 12 && b->x == 88);
        /* the nearest pair first: a new point near the second does not take the first's place */
        f.points[0] = point(20, 20, DGX_DI_NO_ID), f.points[1] = point(14, 14, DGX_DI_NO_ID);
        CHECK(dgx_di_pointer_read(&p, 20) == 2 && a->x == 14 && b->x == 20);
        f.n = 1, f.points[0] = point(16, 16, DGX_DI_NO_ID);
        CHECK(dgx_di_pointer_read(&p, 30) == 1 && a->x == 16 && a->down && b->released);
        f.n = 2, f.points[1] = point(80, 80, DGX_DI_NO_ID);
        CHECK(dgx_di_pointer_read(&p, 40) == 2 && a->x == 16 && finger(&p, 2) && finger(&p, 2)->x == 80);
    }

    /*
     * steadied by time, 30 and 50 ms, a read every 10: blinks at the threshold make no press; a press waits 30 ms, its
     * point where it was first seen; a gap shorter than 50 ms keeps it down where it was; 50 make it up
     */
    {
        fake_t       f = fake(0, 99, 0, 99);
        dgx_screen_t s = screen(100, 100, LR, TB, false);
        dgx_di_pointer_init(&p, &f.device, 0, &s);
        dgx_di_pointer_debounce(&p, 30, 50);
        static const struct {
            int  x; /* seen there; 0: not seen */
            bool down, pressed, released;
        } reads[] = {
            {10, false, false, false}, {0, false, false, false},  /* a blink: seen, not down */
            {11, false, false, false}, {12, false, false, false}, {13, false, false, false},
            {14, true, true, false},   {0, true, false, false},   {0, true, false, false}, /* 30 ms running: down; lost a moment */
            {15, true, false, false},  {0, true, false, false},   {0, true, false, false},
            {0, true, false, false},   {0, true, false, false},   {0, true, false, false},
            {0, false, false, true},   {0, false, false, false}, /* 50 ms running: up */
        };
        bool steady_ok = true;
        for (size_t i = 0; i < sizeof reads / sizeof *reads; ++i) {
            f.n = reads[i].x > 0, f.points[0] = point(reads[i].x, 50, DGX_DI_NO_ID);
            const int              n = dgx_di_pointer_read(&p, 1000 + 10 * (uint32_t)i);
            const dgx_di_finger_t *g = NULL;
            for (int k = 0; k < DGX_DI_FINGERS; ++k)
                if (p.fingers[k].down || p.fingers[k].sees || p.fingers[k].released) g = &p.fingers[k];
            const bool down = g && g->down, pressed = g && g->pressed, released = g && g->released;
            if (n != reads[i].down || down != reads[i].down || pressed != reads[i].pressed || released != reads[i].released) {
                fprintf(stderr, "read %zu: %d down, down %d, pressed %d, released %d\n", i, n, down, pressed, released);
                steady_ok = false;
            }
            if (i == 5 && (g->x != 11 || g->seen_x != 14 || g->landed_x != 11)) steady_ok = false; /* down where first seen */
            if (i == 6 && g->x != 11) steady_ok = false;                                           /* lost a moment: there still */
            if (i == 8 && g->x != 15) steady_ok = false;
            if (i == 14 && g->x != 15) steady_ok = false; /* up: where it went up */
        }
        CHECK(steady_ok);
        /* 0 and 0: as the device says */
        dgx_di_pointer_debounce(&p, -1, 0);
        CHECK(p.down_ms == 0 && p.up_ms == 0);
        int x, y;
        f.n = 1, f.points[0] = point(20, 20, DGX_DI_NO_ID);
        CHECK(dgx_di_pointer_read(&p, 2000) == 1 && dgx_di_pointer_first(&p, &x, &y) && x == 20);
        f.n = 0;
        CHECK(dgx_di_pointer_read(&p, 2001) == 0 && !dgx_di_pointer_first(&p, &x, &y));
        /* the clock past its 32 bits: running on */
        dgx_di_pointer_debounce(&p, 30, 0);
        f.n = 1;
        CHECK(dgx_di_pointer_read(&p, 0xfffffff0u) == 0);
        CHECK(dgx_di_pointer_read(&p, 0xfffffffau) == 0);
        CHECK(dgx_di_pointer_read(&p, 14) == 1); /* 32 ms later */
    }
    CHECK_DONE();
}
