/*
 * The keyboard on the display: its band and keys placed from millimetres (the 7" Guition's display, 1024x600 over
 * 154x86 mm, worked out by hand); a key typed when let go inside it, however long held, nothing when led out; erase
 * again and again when held; shift for one character; pages; several fingers over one another; the fingers on the band claimed, the others
 * not; the band at the top when the field is under it; the queue of what is typed; blobs not of the format refused;
 * the band drawn, a key pressed drawn so and its neighbours not, the signs drawn without a font.
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "check.h"
#include "dgx_di.h"
#include "dgx_di_keyboard.h"
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

enum { W = 1024, H = 600, MM_W = 154, MM_H = 86 };

static fake_t            f = {.device = {.read = fake_read, .fingers = 4, .x_min = 0, .x_max = W - 1, .y_min = 0, .y_max = H - 1}};
static dgx_di_pointer_t  p;
static uint32_t          now;
static uint32_t          typed[64];
static int               n_typed, take_max = 64; /* a take gives at most so many */
static dgx_di_keyboard_t kb;

/* fingers at these places (id, x, y), or none; a read 10 ms later, the keyboard taken: what it typed added to typed */
static void look(int n, const int (*at)[3])
{
    f.n = n;
    for (int i = 0; i < n; ++i) f.points[i] = (dgx_di_point_t){.x = (int16_t)at[i][1], .y = (int16_t)at[i][2], .id = (uint8_t)at[i][0]};
    now += 10;
    dgx_di_pointer_read(&p, now);
    n_typed += dgx_di_keyboard_take(&kb, &p, now, typed + n_typed, take_max < 64 - n_typed ? take_max : 64 - n_typed);
}

static void one(int x, int y)
{
    const int at[1][3] = {{1, x, y}};
    look(1, at);
}

static void none(void)
{
    look(0, NULL);
}

/* the middle of key k of the page shown */
static int mid_x(int k)
{
    return kb.keys[k].x + kb.keys[k].w / 2;
}
static int mid_y(int k)
{
    return kb.keys[k].y + kb.keys[k].h / 2;
}

/* a tap on key k: down, a read held, up */
static void tap(int k)
{
    one(mid_x(k), mid_y(k)), one(mid_x(k), mid_y(k)), none();
}

/* typed since the last call, as a string of code points (the control ones as ^H, ^J, ^[) */
static const char *got(void)
{
    static char s[256];
    int         at = 0;
    for (int i = 0; i < n_typed && at < 250; ++i) {
        const uint32_t c = typed[i];
        if (c == 0x08) at += sprintf(s + at, "^H");
        else if (c == 0x0a) at += sprintf(s + at, "^J");
        else if (c == 0x1b) at += sprintf(s + at, "^[");
        else s[at++] = (char)c;
    }
    s[at]   = 0;
    n_typed = 0;
    return s;
}

static unsigned pixel(dgx_screen_t *s, int x, int y)
{
    return (unsigned)s->get_pixel(s, x, y);
}

int main(void)
{
    dgx_screen_t *display = dgx_vscreen_init(W, H, 16, DgxScreenRGB);
    dgx_di_pointer_init(&p, &f.device, 0, display);

    /* the code: 3 keys of 20 mm at most (133 px) across, 4 rows of 10 mm (70 px), gaps of 1.5 mm (10 px), a margin of 2 mm
       (13 px across, 14 down); at the bottom in the middle */
    CHECK(dgx_di_keyboard_open(&kb, display, MM_W, MM_H, dgx_di_keyboard_digits, dgx_di_keyboard_digits_size, NULL, NULL));
    CHECK(kb.unit == 133 && kb.key_h == 70 && kb.gap_x == 10 && kb.gap_y == 10 && kb.margin_x == 13 && kb.margin_y == 14 && kb.radius == 14);
    CHECK(kb.x == 289 && kb.y == 262 && kb.w == 445 && kb.h == 338 && kb.count == 12);
    CHECK(kb.keys[0].x == 302 && kb.keys[0].y == 276 && kb.keys[0].w == 133 && kb.keys[0].h == 70);
    CHECK(kb.keys[1].x == 445 && kb.keys[2].x == 588 && kb.keys[2].x + kb.keys[2].w + kb.margin_x == kb.x + kb.w);
    CHECK(kb.keys[9].y == 516 && kb.keys[9].y + kb.keys[9].h + kb.margin_y == H);

    /* drawn: the band from its top color down to its bottom one, the keys of characters plain, erase special with its sign */
    const uint16_t back_top = kb.color[DGX_DI_KEYBOARD_BACK_TOP], back_bottom = kb.color[DGX_DI_KEYBOARD_BACK_BOTTOM];
    const uint16_t plain = kb.color[DGX_DI_KEYBOARD_KEY], special = kb.color[DGX_DI_KEYBOARD_SPECIAL], pressed = kb.color[DGX_DI_KEYBOARD_PRESSED];
    const uint16_t label = kb.color[DGX_DI_KEYBOARD_LABEL];
    CHECK(back_top != back_bottom && plain != special && pressed != plain);
    CHECK(pixel(display, kb.x + 2, kb.y) == back_top && pixel(display, kb.x + 2, kb.y + kb.h - 1) == back_bottom);
    CHECK(pixel(display, kb.x - 1, kb.y + 5) == 0 && pixel(display, kb.x + 5, kb.y - 1) == 0); /* nothing outside it */
    CHECK(pixel(display, mid_x(0), mid_y(0)) == plain && pixel(display, kb.keys[0].x, kb.keys[0].y) != plain); /* its corner rounded off */
    CHECK(pixel(display, kb.keys[9].x + 3, mid_y(9)) == special && pixel(display, mid_x(9), mid_y(9)) == label);

    /* a key typed when let go inside it; drawn pressed while held, its neighbour not */
    one(mid_x(0), mid_y(0));
    CHECK(!strcmp(got(), "") && pixel(display, kb.keys[0].x + 3, mid_y(0)) == pressed && pixel(display, mid_x(1), mid_y(1)) == plain);
    CHECK(p.fingers[0].claimed); /* the keyboard's */
    none();
    CHECK(!strcmp(got(), "1") && pixel(display, kb.keys[0].x + 3, mid_y(0)) == plain);
    /* led out and let go: nothing; back in and let go: typed */
    one(mid_x(1), mid_y(1)), one(mid_x(1), mid_y(1) - 60), none();
    CHECK(!strcmp(got(), ""));
    one(mid_x(1), mid_y(1)), one(mid_x(1), mid_y(1) - 60), one(mid_x(1), mid_y(1)), none();
    CHECK(!strcmp(got(), "2"));
    /* a finger down in a gap: the keyboard's, types nothing; one beside the band: the program's first */
    int x, y;
    one(kb.keys[0].x + kb.keys[0].w + kb.gap_x / 2, mid_y(0));
    CHECK(p.fingers[0].claimed && !dgx_di_pointer_first(&p, &x, &y));
    none();
    CHECK(!strcmp(got(), ""));
    one(100, 100);
    CHECK(!p.fingers[0].claimed && dgx_di_pointer_first(&p, &x, &y) && x == 100 && y == 100);
    one(mid_x(0), mid_y(0)), none(); /* went down beside it, over a key after: nothing */
    CHECK(!strcmp(got(), ""));
    /* erase, done */
    tap(9), tap(11);
    CHECK(!strcmp(got(), "^H^J"));
    /* erase held: once at the long press (500 ms), then every 100 ms; nothing more when let go */
    one(mid_x(9), mid_y(9));
    for (int i = 0; i < 49; ++i) one(mid_x(9), mid_y(9)); /* 490 ms */
    CHECK(!strcmp(got(), ""));
    one(mid_x(9), mid_y(9)); /* 500 */
    CHECK(!strcmp(got(), "^H"));
    for (int i = 0; i < 25; ++i) one(mid_x(9), mid_y(9)); /* 750: at 600 and 700 */
    none();
    CHECK(!strcmp(got(), "^H^H"));
    for (int i = 0; i < 20; ++i) none();
    tap(9); /* after it, a while later, a tap: one erase, the repeat gone with the finger */
    CHECK(!strcmp(got(), "^H"));
    /* two fingers over one another: 7 down, 8 down, 7 up, 8 up */
    {
        const int both[2][3] = {{1, mid_x(6), mid_y(6)}, {2, mid_x(7), mid_y(7)}}, second[1][3] = {{2, mid_x(7), mid_y(7)}};
        look(1, both), look(2, both), look(1, second), none();
        CHECK(!strcmp(got(), "78"));
        /* slowly: 7 held long (past a long press) while 8 goes down, both held long, then let go: still typed */
        look(1, both);
        for (int i = 0; i < 60; ++i) look(1, both);
        for (int i = 0; i < 60; ++i) look(2, both);
        look(1, second);
        for (int i = 0; i < 30; ++i) look(1, second);
        none();
        CHECK(!strcmp(got(), "78"));
        /* a key held long alone: typed when let go; led out after a long hold: not */
        one(mid_x(4), mid_y(4));
        for (int i = 0; i < 80; ++i) one(mid_x(4), mid_y(4));
        none();
        CHECK(!strcmp(got(), "5"));
        one(mid_x(4), mid_y(4));
        for (int i = 0; i < 80; ++i) one(mid_x(4), mid_y(4));
        one(mid_x(4), mid_y(4) - 100), none();
        CHECK(!strcmp(got(), ""));
    }
    /* the queue: what does not fit into one take stays for the next */
    {
        uint32_t two[2];
        take_max = 0;
        for (int i = 0; i < 3; ++i) one(mid_x(i), mid_y(i)), none();
        take_max = 64;
        now += 10, dgx_di_pointer_read(&p, now);
        CHECK(dgx_di_keyboard_take(&kb, &p, now, two, 2) == 2 && two[0] == '1' && two[1] == '2');
        CHECK(dgx_di_keyboard_take(&kb, &p, now, two, 2) == 1 && two[0] == '3');
        n_typed = 0;
    }
    /* closed: nothing taken, nothing claimed */
    dgx_di_keyboard_close(&kb);
    one(mid_x(0), mid_y(0));
    CHECK(!p.fingers[0].claimed);
    none();
    CHECK(!strcmp(got(), ""));

    /* the field at the bottom, under the band: the band at the top */
    const dgx_di_rect_t field = {.x = 100, .y = 500, .w = 600, .h = 60};
    CHECK(dgx_di_keyboard_open(&kb, display, MM_W, MM_H, dgx_di_keyboard_digits, dgx_di_keyboard_digits_size, NULL, &field) && kb.y == 0);
    const dgx_di_rect_t beside = {.x = 0, .y = 500, .w = 200, .h = 60};
    CHECK(dgx_di_keyboard_open(&kb, display, MM_W, MM_H, dgx_di_keyboard_digits, dgx_di_keyboard_digits_size, NULL, &beside) && kb.y == 262);

    /* Latin: 10 keys across at most, 93 px each (the 154 mm let 14 mm, not 16), gaps of 8; letters with shift; pages */
    CHECK(dgx_di_keyboard_open(&kb, display, MM_W, MM_H, dgx_di_keyboard_latin, dgx_di_keyboard_latin_size, NULL, NULL));
    CHECK(kb.unit == 93 && kb.gap_x == 8 && kb.key_h == 70 && kb.x == 1 && kb.w == 1022 && kb.y == 276 && kb.h == 324 && kb.count == 33);
    CHECK(kb.keys[19].w == 6 * 93 / 4 && kb.keys[30].w == 20 * 93 / 4); /* shift of 6 quarters, the space bar of 20 */
    tap(0), tap(10);
    CHECK(!strcmp(got(), "qa"));
    tap(19), tap(0), tap(1); /* shift: Q, then w */
    CHECK(!strcmp(got(), "Qw") && !kb.shift);
    tap(19), tap(19), tap(0); /* shift twice: none */
    CHECK(!strcmp(got(), "q"));
    tap(19), tap(28), tap(29), tap(0); /* shift, to the other page and back: shift no more */
    CHECK(!strcmp(got(), "q") && !kb.shift);
    tap(30), tap(31), tap(29), tap(32); /* space, the dot, cancel, done */
    CHECK(!strcmp(got(), " .^[^J"));
    tap(28); /* to the digits and signs */
    CHECK(kb.page == 1 && kb.count == 34 && !strcmp(got(), ""));
    tap(0), tap(19), tap(20);
    CHECK(!strcmp(got(), "1'_"));
    tap(29); /* back */
    CHECK(kb.page == 0 && kb.count == 33);
    tap(0);
    CHECK(!strcmp(got(), "q"));

    /* blobs not of the format: refused, nothing open */
    {
        static uint8_t b[1024];
        const size_t   size = dgx_di_keyboard_latin_size;
        memcpy(b, dgx_di_keyboard_latin, size);
        CHECK(dgx_di_keyboard_open(&kb, display, MM_W, MM_H, b, size, NULL, NULL));
        CHECK(!dgx_di_keyboard_open(&kb, display, MM_W, MM_H, b, 31, NULL, NULL) && !kb.open); /* short of its header */
        CHECK(!dgx_di_keyboard_open(&kb, display, MM_W, MM_H, b, 32 + 12 + 12 * 66, NULL, NULL)); /* short of its keys */
        CHECK(!dgx_di_keyboard_open(&kb, display, MM_W, MM_H, b, 32 + 12 + 12 * 67, NULL, NULL)); /* of its strings */
        CHECK(!dgx_di_keyboard_open(&kb, display, MM_W, MM_H, b, size - 1, NULL, NULL));          /* its name not ended */
        CHECK(!dgx_di_keyboard_open(&kb, display, MM_W, MM_H, NULL, size, NULL, NULL));
        b[0] = 'X';
        CHECK(!dgx_di_keyboard_open(&kb, display, MM_W, MM_H, b, size, NULL, NULL));
        b[0] = 'D', b[4] = 2; /* another version */
        CHECK(!dgx_di_keyboard_open(&kb, display, MM_W, MM_H, b, size, NULL, NULL));
        b[4] = 1, b[32] = 6; /* six rows */
        CHECK(!dgx_di_keyboard_open(&kb, display, MM_W, MM_H, b, size, NULL, NULL));
        b[32] = 4, b[37] = 1; /* a row past the last */
        CHECK(!dgx_di_keyboard_open(&kb, display, MM_W, MM_H, b, size, NULL, NULL));
        b[37] = 0, b[6] = 66; /* not as many keys as the pages have */
        CHECK(!dgx_di_keyboard_open(&kb, display, MM_W, MM_H, b, size, NULL, NULL));
        b[6] = 67, b[8] = 0, b[9] = 0; /* a key of no height */
        CHECK(!dgx_di_keyboard_open(&kb, display, MM_W, MM_H, b, size, NULL, NULL));
        /* the code's four rows told as three, the fourth past them: the keys add up, the rows do not */
        memcpy(b, dgx_di_keyboard_digits, dgx_di_keyboard_digits_size);
        CHECK(dgx_di_keyboard_open(&kb, display, MM_W, MM_H, b, dgx_di_keyboard_digits_size, NULL, NULL));
        b[32] = 3;
        CHECK(!dgx_di_keyboard_open(&kb, display, MM_W, MM_H, b, dgx_di_keyboard_digits_size, NULL, NULL));
        memcpy(b, dgx_di_keyboard_latin, size);
        CHECK(dgx_di_keyboard_open(&kb, display, MM_W, MM_H, b, size, NULL, NULL));
        /* not a 16-bit display; no millimetres; too narrow a display for the keys */
        dgx_screen_t *mono = dgx_vscreen_init(64, 32, 8, DgxScreenRGB);
        CHECK(!dgx_di_keyboard_open(&kb, mono, 20, 10, b, size, NULL, NULL));
        CHECK(!dgx_di_keyboard_open(&kb, display, 0, MM_H, b, size, NULL, NULL));
        dgx_screen_t *narrow = dgx_vscreen_init(40, 600, 16, DgxScreenRGB);
        CHECK(!dgx_di_keyboard_open(&kb, narrow, 6, MM_H, b, size, NULL, NULL));
        dgx_screen_destroy(&mono), dgx_screen_destroy(&narrow);
    }
    dgx_screen_destroy(&display);
    CHECK_DONE();
}
