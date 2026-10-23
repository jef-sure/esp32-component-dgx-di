/*
 * touch_check: a board's touch panel measured and tried on the board itself, step by step, each step told on the
 * display: what to do with the fingers, and what came of it.
 *
 *   - the calibration: dgx_di_calibrate_measure, three rounds of nine targets; printed to the log as C, to paste into
 *     the board's file (board_calibration) and into a program of this board;
 *   - the gestures, with the calibration: a swipe from each edge, four plain swipes, a long press, two fingers at once,
 *     one held while another moves;
 *   - the pad over a moving picture: buttons together, the ways of the cross, a diagonal, a way with a button, START,
 *     BACK, a circle round the cross in one touch.
 *
 * A board without a calibration in its file is measured first; then, and on a board with one, a menu of the three, the
 * results so far above it and in the log (lines with RESULT). The board, its display and its panel come from its file
 * (board.h).
 */
#include <math.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "board.h"
#include "dgx_bits.h"
#include "dgx_di.h"
#include "dgx_di_calibrate.h"
#include "dgx_di_pad.h"
#include "dgx_draw.h"
#include "dgx_font.h"
#include "dgx_fonts.h"
#include "dgx_overlay.h"
#include "drivers/vscreen.h"
#include "esp_log.h"
#include "esp_timer.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

static const char *TAG = "touch_check";

enum { LOOK_MS = 10, TRIES = 3, LINE = 22 };

static dgx_screen_t        *scr;
static dgx_di_device_t     *dev;
static dgx_di_pointer_t     pointer;
static dgx_di_touch_t       touch;
static dgx_di_calibration_t calibration;
static bool                 calibrated, measured; /* measured here, not taken from the board's file */
static int                  W, H;
static float                px_mm; /* pixels a millimetre */

static uint32_t now_ms(void)
{
    return (uint32_t)(esp_timer_get_time() / 1000);
}

static void wait_ms(int ms)
{
    vTaskDelay(pdMS_TO_TICKS(ms));
}

static uint32_t rgb(uint8_t r, uint8_t g, uint8_t b)
{
    switch (scr->color_bits) {
    case 24: return dgx_rgb_to_24(r, g, b);
    case 18: return dgx_rgb_to_18(r, g, b);
    case 12: return dgx_rgb_to_12(r, g, b);
    default: return dgx_rgb_to_16(r, g, b);
    }
}

#define BG     rgb(10, 14, 30)
#define WHITE  rgb(255, 255, 255)
#define YELLOW rgb(255, 220, 0)
#define GREEN  rgb(90, 230, 110)
#define RED    rgb(255, 90, 80)
#define GREY   rgb(150, 150, 160)

/* a line of text with its baseline at y, on any screen (the display, the picture under the pad) */
static void text_on(dgx_screen_t *s, int x, int y, uint32_t color, const char *fmt, ...)
{
    char    buf[160];
    va_list ap;
    va_start(ap, fmt);
    vsnprintf(buf, sizeof buf, fmt, ap);
    va_end(ap);
    dgx_font_string_utf8_screen(s, x, y, buf, color, DgxOutputNormal, 1, dgx_font_check(), NULL, NULL);
}

#define text(...) text_on(scr, __VA_ARGS__)

static int width_of(const char *str)
{
    int16_t top, height;
    return dgx_font_string_bounds(str, dgx_font_check(), &top, &height);
}

/*
 * Text wrapped at its spaces into lines no wider than w, from the baseline y down, LINE apart, on any screen: so a
 * sentence fits a display of any width. The baseline after it.
 */
static int para_on(dgx_screen_t *s, int x, int y, int w, uint32_t color, const char *fmt, ...)
{
    char    buf[256], line[256];
    va_list ap;
    va_start(ap, fmt);
    vsnprintf(buf, sizeof buf, fmt, ap);
    va_end(ap);
    for (const char *p = buf; *p; y += LINE) {
        size_t fit = 0;
        for (size_t end = 0;; ++end) {
            while (p[end] && p[end] != ' ') ++end;
            memcpy(line, p, end), line[end] = 0;
            if (fit && width_of(line) > w) break;
            fit = end;
            if (!p[end]) break;
        }
        memcpy(line, p, fit), line[fit] = 0;
        dgx_font_string_utf8_screen(s, x, y, line, color, DgxOutputNormal, 1, dgx_font_check(), NULL, NULL);
        for (p += fit; *p == ' '; ++p) {}
    }
    return y;
}

#define para(x, y, color, ...) para_on(scr, x, y, W - 2 * (x), color, __VA_ARGS__)

static void clear(void)
{
    dgx_fill_rectangle(scr, 0, 0, W, H, BG);
}

/* the pointer read and the first finger taken by the touch, now */
static int look(void)
{
    const uint32_t t = now_ms();
    const int      n = dgx_di_pointer_read(&pointer, t);
    int            x, y;
    const bool     down = dgx_di_pointer_first(&pointer, &x, &y);
    dgx_di_touch_take(&touch, x, y, down, t);
    return n;
}

/* no finger on the panel for a while: the next step starts from nothing */
static void all_up(void)
{
    for (uint32_t since = now_ms(); now_ms() - since < 300; wait_ms(LOOK_MS))
        if (look()) since = now_ms();
}

/* a button of the check, drawn; its rectangle for dgx_di_rects */
typedef struct {
    dgx_di_rect_t r;
    const char   *label;
    uint32_t      color;
} button_t;

static void button_draw(dgx_screen_t *s, const button_t *b)
{
    dgx_fill_rectangle(s, b->r.x, b->r.y, b->r.w, b->r.h, b->color);
    text_on(s, b->r.x + (b->r.w - width_of(b->label)) / 2, b->r.y + b->r.h / 2 + 6, WHITE, "%s", b->label);
}

/* the button of n clicked */
static int choose(button_t *buttons, int n)
{
    dgx_di_rect_t  rects[4];
    dgx_di_rects_t set;
    for (int i = 0; i < n; ++i) rects[i] = buttons[i].r, button_draw(scr, &buttons[i]);
    dgx_di_rects_init(&set, rects, n);
    for (;; wait_ms(LOOK_MS)) {
        dgx_di_pointer_read(&pointer, now_ms());
        dgx_di_rects_take(&set, &pointer, now_ms());
        for (int i = 0; i < n; ++i)
            if (rects[i].flags & DGX_DI_RECT_CLICKED) return i;
    }
}

/*
 * One button to go on. Before the calibration the panel may lie mirrored under the display: then the button is in the
 * very middle, where the finger is, mirrored or not (the text above it ends by H / 2 - 28); after it, at the bottom.
 */
static void go_on(const char *label)
{
    const int y = calibrated ? H - 64 : H / 2 - 28;
    button_t  b = {{.x = (int16_t)(W / 2 - 90), .y = (int16_t)y, .w = 180, .h = 56}, label, rgb(30, 110, 50)};
    choose(&b, 1);
    all_up();
}

/* ---------------------------------------------------------------- the calibration */

static char code[900];

static void calibrate(void)
{
    clear();
    text(10, 26, YELLOW, "Калибровка: 3 круга по 9 крестов.");
    para(10, 26 + LINE, WHITE, "Нажмите в центр мигающего креста и держите, пока он не погаснет. Потом уберите палец.");
    go_on("Начать");
    dgx_di_calibration_t made;
    const bool           ok = dgx_di_calibrate_measure(dev, scr, 3, &made);
    clear();
    if (!ok) {
        para(10, 26, RED, calibrated ? "Калибровка не вышла: нажатия не легли по всему экрану. Осталась прежняя."
                                     : "Калибровка не вышла: нажатия не легли по всему экрану. Дальше без неё.");
        ESP_LOGE(TAG, "RESULT the calibration failed");
    } else {
        calibration = made, calibrated = measured = true;
        dgx_di_pointer_calibrate(&pointer, &calibration);
        dgx_di_calibration_code(&calibration, code, sizeof code);
        ESP_LOGI(TAG, "RESULT calibration: %s", code);
        text(10, 26, GREEN, "Калибровка готова.");
        para(10, 26 + LINE, WHITE, "Промах в среднем: без неё %d.%d px, с ней %d.%d px.", calibration.off_tenths / 10, calibration.off_tenths % 10,
             calibration.left_tenths / 10, calibration.left_tenths % 10);
    }
    go_on("Дальше");
}

/* ---------------------------------------------------------------- the gestures */

static void arrow(int x0, int y0, int x1, int y1, uint32_t color)
{
    const float dx = (float)(x1 - x0), dy = (float)(y1 - y0), l = sqrtf(dx * dx + dy * dy), ux = dx / l, uy = dy / l;
    dgx_draw_line_thick(scr, x0, y0, x1, y1, 5, color);
    dgx_draw_line_thick(scr, x1, y1, (int)(x1 - 18 * ux + 12 * uy), (int)(y1 - 18 * uy - 12 * ux), 5, color);
    dgx_draw_line_thick(scr, x1, y1, (int)(x1 - 18 * ux - 12 * uy), (int)(y1 - 18 * uy + 12 * ux), 5, color);
    dgx_solid_circle(scr, x0, y0, 9, color); /* where the finger goes down */
}

static void ring(int x, int y, int r, uint32_t color)
{
    for (int i = 0; i < 4; ++i) dgx_draw_circle(scr, x, y, r - i, color);
}

enum { G_EDGE, G_SWIPE, G_LONG, G_TWO, G_HOLD_MOVE };

typedef struct {
    uint8_t     kind, bit;     /* an edge or a way, DGX_DI_EDGE_* or DGX_DI_SWIPE_* */
    int16_t     x0, y0, x1, y1; /* the arrow, in parts of a thousand of the display; a plain swipe starts out of the
                                   bands of the edges (DGX_DI_EDGE_BAND_MM), or it is a swipe from an edge */
    int16_t     ty;            /* the text's first line, the same */
    const char *say;
    const char *name;          /* in the log */
} gesture_t;

static const gesture_t gestures[] = {
    {G_EDGE, DGX_DI_EDGE_TOP, 500, 4, 500, 420, 640, "Поставьте палец на верхний край экрана и проведите вниз, по стрелке.", "swipe from the top edge"},
    {G_EDGE, DGX_DI_EDGE_BOTTOM, 500, 996, 500, 580, 120, "Поставьте палец на нижний край экрана и проведите вверх, по стрелке.", "swipe from the bottom edge"},
    {G_EDGE, DGX_DI_EDGE_LEFT, 4, 650, 350, 650, 120, "Поставьте палец на левый край экрана и проведите вправо, по стрелке.", "swipe from the left edge"},
    {G_EDGE, DGX_DI_EDGE_RIGHT, 996, 650, 650, 650, 120, "Поставьте палец на правый край экрана и проведите влево, по стрелке.", "swipe from the right edge"},
    {G_SWIPE, DGX_DI_SWIPE_UP, 500, 780, 500, 400, 120, "Поставьте палец на точку и проведите вверх, по стрелке.", "swipe up"},
    {G_SWIPE, DGX_DI_SWIPE_DOWN, 500, 400, 500, 780, 120, "Поставьте палец на точку и проведите вниз, по стрелке.", "swipe down"},
    {G_SWIPE, DGX_DI_SWIPE_LEFT, 700, 620, 300, 620, 120, "Поставьте палец на точку и проведите влево, по стрелке.", "swipe left"},
    {G_SWIPE, DGX_DI_SWIPE_RIGHT, 300, 620, 700, 620, 120, "Поставьте палец на точку и проведите вправо, по стрелке.", "swipe right"},
    {G_LONG, 0, 500, 620, 0, 0, 120, "Нажмите в кольцо и держите, пока оно не станет зелёным. Потом уберите.", "long press"},
    {G_TWO, 0, 300, 620, 700, 620, 120, "Поставьте два пальца сразу, по одному в каждое кольцо. Держите.", "two fingers at once"},
    {G_HOLD_MOVE, 0, 180, 620, 0, 0, 120, "Одним пальцем держите левое кольцо. Другим проведите по стрелке справа.", "one held, another moves"},
};
#define GESTURES (int)(sizeof gestures / sizeof gestures[0])

static int gx(int v)
{
    return v * (W - 1) / 1000;
}

static int gy(int v)
{
    return v * (H - 1) / 1000;
}

/* the hold-and-move gesture's arrow: from the middle to the right, 25 mm */
#define HM_X0 (W / 2 - 10)
#define HM_X1 (HM_X0 + (int)(25 * px_mm))

static void gesture_draw(const gesture_t *g, int try)
{
    clear();
    char head[48];
    snprintf(head, sizeof head, "Жест %d из %d%s", (int)(g - gestures) + 1, GESTURES, try ? ", ещё раз" : "");
    const int ty = gy(g->ty) + 18;
    text(10, ty, YELLOW, "%s", head);
    para(10, ty + LINE, WHITE, "%s", g->say);
    const int r = (int)(6 * px_mm);
    switch (g->kind) {
    case G_EDGE:
        /* the edge to start at, lit */
        if (g->bit == DGX_DI_EDGE_TOP) dgx_fill_rectangle(scr, 0, 0, W, 4, YELLOW);
        if (g->bit == DGX_DI_EDGE_BOTTOM) dgx_fill_rectangle(scr, 0, H - 4, W, 4, YELLOW);
        if (g->bit == DGX_DI_EDGE_LEFT) dgx_fill_rectangle(scr, 0, 0, 4, H, YELLOW);
        if (g->bit == DGX_DI_EDGE_RIGHT) dgx_fill_rectangle(scr, W - 4, 0, 4, H, YELLOW);
        /* fall through */
    case G_SWIPE: arrow(gx(g->x0), gy(g->y0), gx(g->x1), gy(g->y1), YELLOW); break;
    case G_LONG: ring(gx(g->x0), gy(g->y0), r, YELLOW); break;
    case G_TWO:
        ring(gx(g->x0), gy(g->y0), r, YELLOW);
        ring(gx(g->x1), gy(g->y1), r, YELLOW);
        break;
    case G_HOLD_MOVE:
        ring(gx(g->x0), gy(g->y0), r, YELLOW);
        arrow(HM_X0, gy(g->y0), HM_X1, gy(g->y0), YELLOW);
        break;
    }
}

/* what a touch of one finger made, in words */
static const char *told(uint8_t edge, uint8_t swipe, bool long_press)
{
    if (edge & DGX_DI_EDGE_TOP) return "свайп от верхнего края";
    if (edge & DGX_DI_EDGE_BOTTOM) return "свайп от нижнего края";
    if (edge & DGX_DI_EDGE_LEFT) return "свайп от левого края";
    if (edge & DGX_DI_EDGE_RIGHT) return "свайп от правого края";
    if (swipe & DGX_DI_SWIPE_UP) return "свайп вверх";
    if (swipe & DGX_DI_SWIPE_DOWN) return "свайп вниз";
    if (swipe & DGX_DI_SWIPE_LEFT) return "свайп влево";
    if (swipe & DGX_DI_SWIPE_RIGHT) return "свайп вправо";
    if (long_press) return "долгое нажатие";
    return "касание без жеста";
}

/* the verdict in a box in the middle, a moment */
static void verdict(bool ok, const char *what, bool again)
{
    const int bx = 20, by = H / 2 - 50, bw = W - 40, bh = 100;
    dgx_fill_rectangle(scr, bx, by, bw, bh, rgb(25, 30, 50));
    dgx_fill_rectangle(scr, bx, by, bw, 3, ok ? GREEN : RED);
    const int y = para_on(scr, bx + 12, by + 30, bw - 24, ok ? GREEN : RED, ok ? "Вышло: %s." : "Не вышло: %s.", what);
    text(bx + 12, y, WHITE, "%s", ok ? "Следующий жест." : again ? "Попробуйте ещё раз." : "Дальше, следующий жест.");
    wait_ms(ok ? 1200 : 2200);
    all_up();
}

/* one finger's gesture: the touch as it went, from down to up */
static bool one_finger(const gesture_t *g, const char **what)
{
    uint8_t edge = 0, swipe = 0;
    bool    long_press = false, any = false;
    for (;; wait_ms(LOOK_MS)) {
        look();
        if (touch.pressed) edge = 0, swipe = 0, long_press = false, any = true;
        edge |= touch.edge, swipe |= touch.swipe;
        if (touch.long_press) {
            long_press = true;
            if (g->kind == G_LONG) ring(gx(g->x0), gy(g->y0), (int)(6 * px_mm), GREEN);
        }
        if (touch.down) dgx_fill_rectangle(scr, touch.x - 1, touch.y - 1, 3, 3, WHITE);
        if (touch.released && any) break;
    }
    *what = told(edge, swipe, long_press);
    switch (g->kind) {
    case G_EDGE: return edge == g->bit && !swipe;
    case G_SWIPE: return swipe == g->bit && !edge;
    default: return long_press && !edge && !swipe;
    }
}

static int dist(int x0, int y0, int x1, int y1)
{
    return (int)sqrtf((float)((x1 - x0) * (x1 - x0) + (y1 - y0) * (y1 - y0)));
}

/* two fingers at once, each in its ring, held a moment */
static bool two_fingers(const gesture_t *g, const char **what)
{
    static char said[64];
    const int   near = (int)(9 * px_mm);
    int         most = 0;
    uint32_t    both = 0;
    bool        any  = false;
    for (;; wait_ms(LOOK_MS)) {
        const int n = look();
        bool      in0 = false, in1 = false;
        for (int i = 0; i < DGX_DI_FINGERS; ++i) {
            const dgx_di_finger_t *f = &pointer.fingers[i];
            if (!f->down) continue;
            dgx_fill_rectangle(scr, f->x - 1, f->y - 1, 3, 3, WHITE);
            in0 |= dist(f->x, f->y, gx(g->x0), gy(g->y0)) < near;
            in1 |= dist(f->x, f->y, gx(g->x1), gy(g->y1)) < near;
        }
        if (n) any = true;
        if (n > most) most = n;
        if (n == 2 && in0 && in1) {
            if (!both) both = now_ms();
            if (now_ms() - both >= 300) {
                *what = "два пальца в кольцах";
                return true;
            }
        } else {
            both = 0;
        }
        if (any && !n) break;
    }
    snprintf(said, sizeof said, most < 2 ? "видно не больше %d пальца сразу" : "%d пальца, но не в кольцах", most);
    *what = said;
    return false;
}

/* one finger held in the ring, another gone along the arrow meanwhile, the held one neither lost nor moved */
static bool hold_and_move(const gesture_t *g, const char **what)
{
    const int rx = gx(g->x0), ry = gy(g->y0), near = (int)(9 * px_mm), travel = (int)(DGX_DI_SWIPE_TRAVEL_MM * px_mm);
    int       held = -1, mover = -1; /* fingers' indices */
    bool      any = false, lost = false, strayed = false;
    for (;; wait_ms(LOOK_MS)) {
        const int n = look();
        if (n) any = true;
        for (int i = 0; i < DGX_DI_FINGERS; ++i) {
            const dgx_di_finger_t *f = &pointer.fingers[i];
            if (f->down) dgx_fill_rectangle(scr, f->x - 1, f->y - 1, 3, 3, WHITE);
            if (f->pressed && held < 0 && dist(f->x, f->y, rx, ry) < near) held = i;
            else if (f->pressed && held >= 0 && mover < 0) mover = i;
        }
        if (held >= 0) {
            const dgx_di_finger_t *h = &pointer.fingers[held];
            if (!h->down) {
                if (mover >= 0) lost = true;
                held = -1, mover = -1;
            } else if (dist(h->x, h->y, rx, ry) >= near) {
                strayed = true;
            }
        }
        if (held >= 0 && mover >= 0) {
            const dgx_di_finger_t *m = &pointer.fingers[mover];
            if (!m->down) mover = -1;
            else if (m->x - m->landed_x >= travel && !strayed) {
                *what = "один держит, другой прошёл";
                return true;
            }
        }
        if (any && !n) break;
    }
    *what = lost ? "держащий палец потерян" : strayed ? "держащий палец уехал из кольца" : "второй палец не прошёл 15 мм";
    return false;
}

static int  gestures_passed;
static bool gestures_done;

static void gestures_check(void)
{
    clear();
    text(10, 26, YELLOW, "Жесты: %d штук.", GESTURES);
    para(10, 26 + LINE, WHITE, "Что сделать, будет написано и нарисовано на экране. На каждый жест 3 попытки.");
    go_on("Начать");
    gestures_passed = 0;
    for (int k = 0; k < GESTURES; ++k) {
        const gesture_t *g = &gestures[k];
        for (int try = 0; try < TRIES; ++try) {
            gesture_draw(g, try);
            const char *what = "";
            const bool  ok   = g->kind == G_TWO ? two_fingers(g, &what) : g->kind == G_HOLD_MOVE ? hold_and_move(g, &what) : one_finger(g, &what);
            ESP_LOGI(TAG, "gesture %d (%s), try %d: %s, told: %s", k + 1, g->name, try + 1, ok ? "ok" : "no", what);
            verdict(ok, what, try + 1 < TRIES);
            if (ok) {
                ++gestures_passed;
                break;
            }
        }
    }
    gestures_done = true;
    ESP_LOGI(TAG, "RESULT gestures: %d of %d", gestures_passed, GESTURES);
}

/* ---------------------------------------------------------------- the pad */

enum { CROSS = DGX_DI_PAD_UP | DGX_DI_PAD_DOWN | DGX_DI_PAD_LEFT | DGX_DI_PAD_RIGHT, ABXY = DGX_DI_PAD_A | DGX_DI_PAD_B | DGX_DI_PAD_X | DGX_DI_PAD_Y };

typedef struct {
    uint32_t    want; /* held together, nothing else; 0: the circle */
    const char *say;
} pad_task_t;

static const pad_task_t pad_tasks[] = {
    {ABXY, "Нажмите и держите вместе A, B, X и Y."},
    {DGX_DI_PAD_UP, "Нажмите на кресте вверх и держите."},
    {DGX_DI_PAD_DOWN, "Нажмите на кресте вниз и держите."},
    {DGX_DI_PAD_LEFT, "Нажмите на кресте влево и держите."},
    {DGX_DI_PAD_DOWN | DGX_DI_PAD_LEFT, "Нажмите на кресте вниз-влево, в угол."},
    {DGX_DI_PAD_RIGHT | DGX_DI_PAD_A, "Держите на кресте вправо и кнопку A."},
    {DGX_DI_PAD_START, "Нажмите START (правая маленькая)."},
    {DGX_DI_PAD_BACK, "Нажмите BACK (левая маленькая)."},
    {0, "Обведите крест пальцем по кругу, не отрывая."},
};
#define PAD_TASKS (int)(sizeof pad_tasks / sizeof pad_tasks[0])

static void held_names(uint32_t b, char *out, size_t size)
{
    static const struct {
        uint32_t    bit;
        const char *name;
    } names[] = {{DGX_DI_PAD_UP, "↑"},  {DGX_DI_PAD_DOWN, "↓"}, {DGX_DI_PAD_LEFT, "←"},      {DGX_DI_PAD_RIGHT, "→"},   {DGX_DI_PAD_A, "A"},
                 {DGX_DI_PAD_B, "B"},   {DGX_DI_PAD_X, "X"},    {DGX_DI_PAD_Y, "Y"},        {DGX_DI_PAD_START, "START"}, {DGX_DI_PAD_BACK, "BACK"}};
    out[0] = 0;
    for (size_t i = 0; i < sizeof names / sizeof names[0]; ++i)
        if (b & names[i].bit) snprintf(out + strlen(out), size - strlen(out), "%s%s", out[0] ? " + " : "", names[i].name);
    if (!out[0]) snprintf(out, size, "ничего");
}

static int  pad_passed;
static bool pad_failed, pad_done;

static void pad_check(void)
{
    clear();
    text(10, 26, YELLOW, "Пад: %d заданий.", PAD_TASKS);
    para(10, 26 + LINE, WHITE, "Задание будет написано вверху. Получилось - сразу следующее. Не выходит - кнопка «Пропустить» справа вверху.");
    go_on("Начать");
    dgx_screen_t *pic = dgx_vscreen_init(W, H, scr->color_bits, scr->rgb_order);
    dgx_di_pad_t  pad;
    const bool    made = pic && (board_pad ? dgx_di_pad_init(&pad, scr, board_mm_w, board_mm_h, board_pad, board_pad_count)
                                           : dgx_di_pad_init(&pad, scr, board_mm_w, board_mm_h, dgx_di_pad_classic, DGX_DI_PAD_CLASSIC));
    if (!made || !dgx_di_pad_show(&pad, scr, 160)) {
        ESP_LOGE(TAG, "RESULT pad: no memory for it");
        if (made) dgx_di_pad_free(&pad, scr);
        dgx_screen_destroy(&pic);
        pad_failed = true;
        return;
    }
    pad_passed = 0;
    button_t      skip = {{.x = (int16_t)(W - 136), .y = 4, .w = 132, .h = 34}, "Пропустить", rgb(110, 40, 40)};
    dgx_di_rects_t rects;
    dgx_di_rects_init(&rects, &skip.r, 1);
    int ball_x = W / 2, ball_y = H / 2, vx = 3, vy = 2;
    for (int k = 0; k < PAD_TASKS; ++k) {
        const pad_task_t *task = &pad_tasks[k];
        uint32_t          since = 0, done = 0, ways = 0;
        bool              skipped = false;
        for (;; wait_ms(LOOK_MS)) {
            /* the picture moves under the pad: a ball across stripes */
            ball_x += vx, ball_y += vy;
            if (ball_x < 20 || ball_x > W - 20) vx = -vx;
            if (ball_y < 20 || ball_y > H - 20) vy = -vy;
            for (int y = 0; y < H; y += 34) dgx_fill_rectangle(pic, 0, y, W, 17, rgb(20, 40, 70)), dgx_fill_rectangle(pic, 0, y + 17, W, 17, rgb(30, 55, 90));
            dgx_solid_circle(pic, ball_x, ball_y, 18, rgb(240, 120, 40));
            /* the task, what is held, the skip */
            char held[64];
            held_names(pad.buttons, held, sizeof held);
            text_on(pic, 10, 26, YELLOW, "Задание %d из %d", k + 1, PAD_TASKS);
            const int y = para_on(pic, 10, 26 + LINE + 4, W - 20, WHITE, "%s", task->say);
            text_on(pic, 10, y, done ? GREEN : GREY, done ? "Вышло!" : "Нажато: %s", held);
            button_draw(pic, &skip);
            dgx_vscreen_to_screen(scr, 0, 0, pic, 0);
            int dx, dy, dw, dh;
            dgx_overlay_take_dirty(scr, &dx, &dy, &dw, &dh); /* the whole picture goes every frame */

            dgx_di_pointer_read(&pointer, now_ms());
            dgx_di_pad_take(&pad, &pointer);
            dgx_di_rects_take(&rects, &pointer, now_ms());
            if (done) {
                if (now_ms() - done > 900) break;
                continue;
            }
            if (skip.r.flags & DGX_DI_RECT_CLICKED) {
                skipped = true;
                break;
            }
            bool ok;
            if (task->want) {
                ok = pad.buttons == task->want;
            } else {
                /* the circle: every way of the cross in one touch of it */
                if (!(pad.buttons & CROSS)) ways = 0;
                ways |= pad.buttons & CROSS;
                ok = ways == CROSS;
            }
            if (!ok) since = 0;
            else if (!since) since = now_ms();
            if (ok && (!task->want || now_ms() - since >= 300)) done = now_ms();
        }
        ESP_LOGI(TAG, "pad task %d: %s", k + 1, skipped ? "skipped" : "ok");
        if (!skipped) ++pad_passed;
        /* the next from nothing held */
        while (look() || pad.buttons) dgx_di_pad_take(&pad, &pointer), wait_ms(LOOK_MS);
    }
    pad_done = true;
    ESP_LOGI(TAG, "RESULT pad: %d of %d", pad_passed, PAD_TASKS);
    dgx_di_pad_free(&pad, scr);
    dgx_screen_destroy(&pic);
}

/* ---------------------------------------------------------------- all of it */

void app_main(void)
{
    scr = board_display();
    if (!scr) {
        ESP_LOGE(TAG, "no display");
        return;
    }
    W = scr->width, H = scr->height, px_mm = (float)W / (float)board_mm_w;
    clear();
    para(10, 26, YELLOW, "Проверка тача: %s", board_name);
    dev = board_touch();
    if (!dev) {
        para(10, 26 + 2 * LINE, RED, "Тач не отвечает (подробности в журнале).");
        return;
    }
    dgx_di_pointer_init(&pointer, dev, 0, scr);
    dgx_di_touch_display(&touch, scr, board_mm_w, board_mm_h);
    if (board_calibration && dgx_di_pointer_calibrate(&pointer, board_calibration)) {
        calibration = *board_calibration, calibrated = true;
    } else {
        if (board_calibration) ESP_LOGE(TAG, "the board's calibration is not for this panel and display: measured anew");
        para(10, 26 + 2 * LINE, WHITE, "Сначала калибровка, потом можно выбрать жесты или пад. Что делать, будет на экране.");
        go_on("Начать");
        calibrate();
    }
    for (;;) {
        clear();
        int y = para(10, 26, YELLOW, "Проверка тача: %s", board_name);
        if (calibrated) {
            y = para(10, y, WHITE, "Калибровка %s: промах без неё %d.%d px, с ней %d.%d px.", measured ? "измерена сейчас" : "из файла платы",
                     calibration.off_tenths / 10, calibration.off_tenths % 10, calibration.left_tenths / 10, calibration.left_tenths % 10);
            if (measured) ESP_LOGI(TAG, "RESULT calibration: %s", code);
        } else {
            y = para(10, y, RED, "Калибровки нет.");
        }
        if (gestures_done) y = para(10, y, WHITE, "Жесты: %d из %d.", gestures_passed, GESTURES);
        else y = para(10, y, GREY, "Жесты не проверялись.");
        if (pad_done) y = para(10, y, WHITE, "Пад: %d из %d.", pad_passed, PAD_TASKS);
        else y = para(10, y, GREY, pad_failed ? "Пад не сделан: не хватило памяти." : "Пад не проверялся.");
        ESP_LOGI(TAG, "RESULT gestures %d of %d, pad %d of %d", gestures_passed, gestures_done ? GESTURES : 0, pad_passed, pad_done ? PAD_TASKS : 0);
        /* the menu, at the bottom: the pointer is calibrated by now, or the board has no calibration at all */
        const int bw = (W - 40) / 3;
        button_t  menu[3];
        static const char *const labels[3] = {"Калибровка", "Жесты", "Пад"};
        for (int i = 0; i < 3; ++i) menu[i] = (button_t){{.x = (int16_t)(10 + i * (bw + 10)), .y = (int16_t)(H - 66), .w = (int16_t)bw, .h = 56}, labels[i], rgb(30, 70, 120)};
        text(10, H - 76, YELLOW, "Что проверить?");
        const int c = choose(menu, 3);
        all_up();
        if (c == 0) calibrate();
        else if (c == 1) gestures_check();
        else pad_check();
    }
}
