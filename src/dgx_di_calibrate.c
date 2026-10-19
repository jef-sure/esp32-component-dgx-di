/*
 * The calibration measured on a board (dgx_di_calibrate.h), as dgx-ge-prince's touch test measured the 10" Guition's:
 * targets to touch, where the touches land, round after round.
 */
#include "dgx_di_calibrate.h"

#include <stdlib.h>

#include "dgx_bits.h"
#include "dgx_draw.h"
#include "esp_log.h"
#include "esp_timer.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

static const char *TAG = "dgx_di_calibrate";

/*
 * nine targets; the arms of their crosses; how they blink; how often the device is read; a press, a hold, a lift; the
 * touches of a target kept, the last so many rounds' (their median makes it: a touch beside the target does not move it)
 */
enum { AIMS = 9, ARM = 30, BLINK_MS = 250, LOOK_MS = 10, PRESS_MS = 100, HOLD_MS = 400, LIFT_MS = 300, LANDING_MS = 100, KEPT = 15, TEXT = 900 };

static uint32_t now_ms(void)
{
    return (uint32_t)(esp_timer_get_time() / 1000);
}

static uint32_t rgb(const dgx_screen_t *s, uint8_t r, uint8_t g, uint8_t b)
{
    switch (s->color_bits) {
    case 24: return dgx_rgb_to_24(r, g, b);
    case 18: return dgx_rgb_to_18(r, g, b);
    case 16: return dgx_rgb_to_16(r, g, b);
    case 12: return dgx_rgb_to_12(r, g, b);
    default: return r || g || b;
    }
}

/* a cross with a square in its middle, drawn as one batch */
static void target(dgx_screen_t *s, int x, int y, uint32_t ink, uint32_t heart)
{
    dgx_screen_progress_up(s);
    dgx_fill_rectangle(s, x - ARM, y - 2, 2 * ARM + 1, 5, ink);
    dgx_fill_rectangle(s, x - 2, y - ARM, 5, 2 * ARM + 1, ink);
    dgx_fill_rectangle(s, x - 6, y - 6, 13, 13, heart);
    dgx_screen_progress_down(s);
}

/* where the press is taken: a thick ring round it */
static void ring(dgx_screen_t *s, int x, int y, uint32_t ink)
{
    dgx_screen_progress_up(s);
    dgx_fill_rectangle(s, x - 9, y - 9, 19, 5, ink);
    dgx_fill_rectangle(s, x - 9, y + 5, 19, 5, ink);
    dgx_fill_rectangle(s, x - 9, y - 4, 5, 9, ink);
    dgx_fill_rectangle(s, x + 5, y - 4, 5, 9, ink);
    dgx_screen_progress_down(s);
}

/* the device now: the first finger it sees, if any */
static bool look(dgx_di_device_t *d, int *x, int *y)
{
    dgx_di_point_t pts[DGX_DI_FINGERS];
    const int      n = d->read(d, pts, !d->fingers ? 1 : d->fingers < DGX_DI_FINGERS ? d->fingers : DGX_DI_FINGERS);
    if (n > 0) *x = pts[0].x, *y = pts[0].y;
    return n > 0;
}

/* the median of n values (the middle two's mean, rounded, when n is even) */
static int16_t median(const int16_t *v, int n)
{
    int16_t s[KEPT];
    for (int i = 0; i < n; ++i) {
        int j = i;
        for (; j > 0 && s[j - 1] > v[i]; --j) s[j] = s[j - 1];
        s[j] = v[i];
    }
    return n % 2 ? s[n / 2] : (int16_t)((s[n / 2 - 1] + s[n / 2] + 1) >> 1);
}

/* a point of the screen as it stands, in the display's own orientation (bus/dgx_fb_area_priv.h of DGX) */
static void own(const dgx_screen_t *s, int x, int y, int16_t *ox, int16_t *oy)
{
    const int w = s->swap_xy ? s->height : s->width, h = s->swap_xy ? s->width : s->height;
    const int ax = s->swap_xy ? y : x, ay = s->swap_xy ? x : y;
    *ox = (int16_t)(s->dir_x == DgxScreenRightLeft ? w - 1 - ax : ax), *oy = (int16_t)(s->dir_y == DgxScreenBottomTop ? h - 1 - ay : ay);
}

/*
 * A touch on the target at x, y: the device's point on average while it is held; the longest it showed a finger before
 * a press (a blink) and lost one held (a gap). The last showing before the press, lost for less than LANDING_MS, is the
 * finger landing, not near the glass: no blink (on the 10" it made 99 ms of one, and 120 ms before every press). Each
 * blink and landing logged. shown: a pointer to show where the panel sees it, or NULL.
 */
static bool aim(dgx_di_device_t *d, dgx_screen_t *s, const dgx_di_pointer_t *shown, int x, int y, int *rx, int *ry, uint32_t *blink, uint32_t *gap)
{
    const uint32_t white = rgb(s, 255, 255, 255), black = rgb(s, 0, 0, 0), red = rgb(s, 255, 0, 0);
    int            px, py;
    /* a finger still down from before does not count: it comes down on this target */
    for (uint32_t gone = now_ms(); now_ms() - gone < LIFT_MS; vTaskDelay(pdMS_TO_TICKS(LOOK_MS)))
        if (look(d, &px, &py)) gone = now_ms();
    const uint32_t from  = now_ms();
    int            phase = -1;
    uint32_t       run   = 0, shown_ms = 0, gone_at = 0; /* the last showing that ended: how long, when it ended */
    bool           seeing = false;
    for (;; vTaskDelay(pdMS_TO_TICKS(LOOK_MS))) {
        const uint32_t t = now_ms();
        if ((int)((t - from) / BLINK_MS % 2) != phase) {
            phase = (int)((t - from) / BLINK_MS % 2);
            target(s, x, y, phase ? black : white, phase ? black : red);
        }
        const bool sees = look(d, &px, &py);
        if (sees && !seeing) {
            /* seen again: the showing before it was a blink unless this is the press it landed into (told at the press) */
            if (shown_ms && t - gone_at >= LANDING_MS) {
                ESP_LOGI(TAG, "near the glass: seen %u ms, gone %u", (unsigned)shown_ms, (unsigned)(t - gone_at));
                if (shown_ms > *blink) *blink = shown_ms;
                shown_ms = 0;
            }
            run = t, seeing = true;
        }
        if (!sees && seeing) {
            if (shown_ms) {
                /* two showings and no press between: the earlier one a blink */
                ESP_LOGI(TAG, "near the glass: seen %u ms, gone %u", (unsigned)shown_ms, (unsigned)(run - gone_at));
                if (shown_ms > *blink) *blink = shown_ms;
            }
            shown_ms = t - run, gone_at = t, seeing = false;
        }
        if (sees && shown) {
            int sx, sy;
            dgx_di_pointer_map(shown, px, py, &sx, &sy, NULL);
            dgx_fill_rectangle(s, sx - 1, sy - 1, 3, 3, white);
        }
        if (sees && t - run >= PRESS_MS) {
            if (shown_ms && run - gone_at < LANDING_MS) {
                ESP_LOGI(TAG, "landing: seen %u ms, lost %u, then pressed", (unsigned)shown_ms, (unsigned)(run - gone_at));
            } else if (shown_ms) {
                ESP_LOGI(TAG, "near the glass: seen %u ms, gone %u", (unsigned)shown_ms, (unsigned)(run - gone_at));
                if (shown_ms > *blink) *blink = shown_ms;
            }
            break;
        }
    }
    target(s, x, y, black, black);
    /* held: its points while it stays, the gaps within */
    int64_t  sx = 0, sy = 0;
    int      count = 0;
    uint32_t held = now_ms(), lost = 0;
    bool     losing = false;
    for (;; vTaskDelay(pdMS_TO_TICKS(LOOK_MS))) {
        const uint32_t t = now_ms();
        if (look(d, &px, &py)) {
            if (losing && t - lost > *gap) *gap = t - lost;
            losing = false, sx += px, sy += py, ++count;
        } else if (!losing) {
            lost = t, losing = true;
        }
        if ((losing && t - lost >= LIFT_MS) || t - held >= HOLD_MS) break;
    }
    if (!count) return false;
    *rx = (int)((2 * sx + count) / (2 * count)), *ry = (int)((2 * sy + count) / (2 * count));
    return true;
}

bool dgx_di_calibrate_measure(dgx_di_device_t *device, dgx_screen_t *screen, int rounds, dgx_di_calibration_t *calibration)
{
    if (!device || !device->read || !screen) return false;
    const int w = screen->width, h = screen->height, ow = screen->swap_xy ? h : w, oh = screen->swap_xy ? w : h;
    const int e = (w < h ? w : h) / 8 < 48 ? (w < h ? w : h) / 8 : 48;
    const int xs[3] = {e, w / 2, w - 1 - e}, ys[3] = {e, h / 2, h - 1 - e};
    /* the corners, the middles of the edges, the middle */
    static const uint8_t at[AIMS][2] = {{0, 0}, {2, 0}, {2, 2}, {0, 2}, {1, 0}, {2, 1}, {1, 2}, {0, 1}, {1, 1}};
    dgx_di_aim_t aims[AIMS];
    int          count[AIMS] = {0};
    /* the touches kept and the text: on the heap, the stack of a small chip's task is short */
    struct kept {
        int16_t x[AIMS][KEPT], y[AIMS][KEPT];
        char    text[TEXT];
    } *kept = malloc(sizeof *kept);
    if (!kept) return false;
    int16_t(*kept_x)[KEPT] = kept->x, (*kept_y)[KEPT] = kept->y;
    char *text             = kept->text;
    uint32_t             blink = 0, gap = 0;
    dgx_di_calibration_t c;
    bool                 have = false;
    const uint32_t       black = rgb(screen, 0, 0, 0), green = rgb(screen, 0, 255, 0), yellow = rgb(screen, 255, 255, 0);
    ESP_LOGI(TAG, "display %dx%d (%dx%d its own): touch each blinking target and hold a moment", w, h, ow, oh);
    for (int round = 1; !rounds || round <= rounds; ++round) {
        dgx_fill_rectangle(screen, 0, 0, w, h, black);
        dgx_di_pointer_t shown;
        if (have) dgx_di_pointer_init(&shown, device, c.flags, screen), dgx_di_pointer_calibrate(&shown, &c);
        int sum = 0, worst = 0;
        for (int i = 0; i < AIMS; ++i) {
            const int x = xs[at[i][0]], y = ys[at[i][1]];
            int       rx, ry;
            if (!aim(device, screen, have ? &shown : NULL, x, y, &rx, &ry, &blink, &gap)) {
                --i; /* lifted before a point was taken: the same target again */
                continue;
            }
            kept_x[i][count[i] % KEPT] = (int16_t)rx, kept_y[i][count[i] % KEPT] = (int16_t)ry, ++count[i];
            if (have) {
                int tx, ty;
                dgx_di_pointer_map(&shown, rx, ry, &tx, &ty, NULL);
                ring(screen, tx, ty, green);
                const int dx = tx - x, dy = ty - y, o = abs(dx) > abs(dy) ? abs(dx) : abs(dy);
                sum += o, worst = o > worst ? o : worst;
                ESP_LOGI(TAG, "round %d, target %d at %d, %d: the device's %d, %d, taken at %d, %d, off by %+d, %+d", round, i + 1, x, y, rx, ry, tx, ty, dx, dy);
            } else {
                ring(screen, x, y, yellow);
                ESP_LOGI(TAG, "round %d, target %d at %d, %d: the device's %d, %d", round, i + 1, x, y, rx, ry);
            }
        }
        if (have) ESP_LOGI(TAG, "round %d: off by %d.%d pixels on average, %d at worst, by the rounds before", round, 10 * sum / AIMS / 10, 10 * sum / AIMS % 10, worst);
        /* the calibration of the rounds so far: each target where the device took it, the median of its touches */
        for (int i = 0; i < AIMS; ++i) {
            const int n = count[i] < KEPT ? count[i] : KEPT;
            own(screen, xs[at[i][0]], ys[at[i][1]], &aims[i].meant_x, &aims[i].meant_y);
            aims[i].x = median(kept_x[i], n), aims[i].y = median(kept_y[i], n);
        }
        if (dgx_di_calibration_make(&c, device, ow, oh, aims, AIMS)) {
            c.rounds = (uint8_t)(round < 255 ? round : 255);
            dgx_di_calibration_steady(&c, (int)blink, (int)gap);
            have = true;
            dgx_di_calibration_text(&c, text, TEXT);
            ESP_LOGI(TAG, "%s", text);
            dgx_di_calibration_code(&c, text, TEXT);
            ESP_LOGI(TAG, "the calibration of %d round%s: %s", round, round == 1 ? "" : "s", text);
        } else {
            ESP_LOGW(TAG, "round %d: the targets make no calibration (do they span the display?)", round);
        }
        vTaskDelay(pdMS_TO_TICKS(1500)); /* the rings seen, then round again */
    }
    free(kept);
    if (have && calibration) *calibration = c;
    return have;
}
