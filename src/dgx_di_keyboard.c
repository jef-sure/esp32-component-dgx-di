#include "dgx_di_keyboard.h"

#include <string.h>

#include "dgx_draw.h"
#include "dgx_font.h"

/*
 * The blob is that of tools/keyboard_blob.py (its format is told there), read where it lies, byte by byte: little-endian
 * and maybe not aligned. The geometry is integer, step for step as tools/keyboard-editor/Layout.js has it (place, lay):
 * the editor shows the keys where they will be. A change here is a change there; the host tests compare them.
 */
enum { HEADER = 32, PAGE_BYTES = 6, KEY_BYTES = 12, NONE = 0xffff };

/* a key of the layout as the blob has it */
typedef struct {
    uint32_t    code, upper;
    const char *label;
    int         width; /* quarters */
} layout_key_t;

static uint16_t u16(const uint8_t *p)
{
    return (uint16_t)(p[0] | p[1] << 8);
}

static uint32_t u32(const uint8_t *p)
{
    return (uint32_t)p[0] | (uint32_t)p[1] << 8 | (uint32_t)p[2] << 16 | (uint32_t)p[3] << 24;
}

static const uint8_t *key_bytes(const dgx_di_keyboard_t *kb, int page, int k)
{
    return kb->layout + HEADER + PAGE_BYTES * kb->pages + KEY_BYTES * (kb->first[page] + k);
}

static layout_key_t key_of(const dgx_di_keyboard_t *kb, int page, int k)
{
    const uint8_t *p     = key_bytes(kb, page, k);
    const uint16_t label = u16(p + 8);
    const size_t   keys  = u16(kb->layout + 6), strings = HEADER + (size_t)PAGE_BYTES * kb->pages + (size_t)KEY_BYTES * keys;
    return (layout_key_t){.code = u32(p), .upper = u32(p + 4), .label = label == NONE ? NULL : (const char *)kb->layout + strings + label, .width = p[10] ? p[10] : 4};
}

static int quarters(const dgx_di_keyboard_t *kb, int page, int k)
{
    const int w = key_bytes(kb, page, k)[10];
    return w ? w : 4;
}

/* a string of the blob ends within it */
static bool ended(const uint8_t *b, size_t size, size_t at)
{
    for (; at < size; ++at)
        if (!b[at]) return true;
    return false;
}

/* the blob read: its pages and their rows, its colors; false when it is no blob of these limits */
static bool parse(dgx_di_keyboard_t *kb, const uint8_t *b, size_t size)
{
    if (!b || size < HEADER || memcmp(b, "DGXK", 4) || b[4] != 1) return false;
    const int pages = b[5], keys = u16(b + 6);
    if (pages < 1 || pages > DGX_DI_KEYBOARD_PAGES) return false;
    const size_t strings = HEADER + (size_t)PAGE_BYTES * pages + (size_t)KEY_BYTES * keys;
    if (strings > size) return false;
    int all = 0;
    for (int p = 0; p < pages; ++p) {
        const uint8_t *page = b + HEADER + PAGE_BYTES * p;
        int            n    = 0;
        if (page[0] < 1 || page[0] > DGX_DI_KEYBOARD_ROWS) return false;
        for (int r = 0; r < DGX_DI_KEYBOARD_ROWS; ++r) {
            if (r < page[0] ? page[1 + r] < 1 : page[1 + r] != 0) return false;
            kb->row[p][r] = page[1 + r], n += page[1 + r];
        }
        if (n > DGX_DI_KEYBOARD_KEYS) return false;
        kb->rows_of[p] = page[0], kb->first[p] = (uint16_t)all, all += n;
    }
    if (all != keys) return false;
    for (int k = 0; k <= keys; ++k) {
        /* the labels, then the name: each ended within the blob */
        const uint16_t at = k < keys ? u16(b + HEADER + PAGE_BYTES * pages + KEY_BYTES * k + 8) : u16(b + 30);
        if (at != NONE && !ended(b, size, strings + at)) return false;
    }
    /* sizes of int16_t: the key at least a tenth of a millimetre */
    for (int i = 0; i < 5; ++i)
        if (u16(b + 8 + 2 * i) > 32767 || (i < 2 && !u16(b + 8 + 2 * i))) return false;
    for (int i = 0; i < DGX_DI_KEYBOARD_COLORS; ++i) kb->color[i] = u16(b + 18 + 2 * i);
    kb->layout = b, kb->pages = (uint8_t)pages;
    return true;
}

/* tenths of a millimetre in pixels along an axis of px pixels over mm millimetres, rounded */
static int pixels(int tenths, int px, int mm)
{
    return (int)(((int64_t)tenths * px * 2 + 10 * mm) / (20 * (int64_t)mm));
}

static bool special(uint32_t code)
{
    return code < 0x20 || code >= DGX_DI_KEY_SHIFT;
}

/* the quarters of a row of a page, its keys from first on */
static int row_quarters(const dgx_di_keyboard_t *kb, int page, int first, int n)
{
    int q = 0;
    for (int i = 0; i < n; ++i) q += quarters(kb, page, first + i);
    return q;
}

/* the band: its size in pixels from the layout's in millimetres, the widest row of all pages fitted into the display */
static bool place(dgx_di_keyboard_t *kb, int width_mm, int height_mm, const dgx_di_rect_t *field)
{
    const uint8_t *b = kb->layout;
    const int      W = kb->display->width, H = kb->display->height;
    const int      key_h_t = u16(b + 8), unit_t = u16(b + 10), gap_t = u16(b + 12), margin_t = u16(b + 14), radius_t = u16(b + 16);
    int            unit = pixels(unit_t, W, width_mm), inner = 0, rows = 0;
    kb->gap_x = (int16_t)pixels(gap_t, W, width_mm), kb->gap_y = (int16_t)pixels(gap_t, H, height_mm);
    kb->margin_x = (int16_t)pixels(margin_t, W, width_mm), kb->margin_y = (int16_t)pixels(margin_t, H, height_mm);
    kb->radius = (int16_t)((pixels(radius_t, W, width_mm) + pixels(radius_t, H, height_mm) + 1) / 2);
    for (int p = 0; p < kb->pages; ++p) {
        for (int r = 0, first = 0; r < kb->rows_of[p]; first += kb->row[p][r++]) {
            const int room = W - 2 * kb->margin_x - (kb->row[p][r] - 1) * kb->gap_x;
            const int fit  = room > 0 ? room * 4 / row_quarters(kb, p, first, kb->row[p][r]) : 0;
            if (fit < unit) unit = fit;
        }
        if (kb->rows_of[p] > rows) rows = kb->rows_of[p];
    }
    if (unit < 1) return false;
    for (int p = 0; p < kb->pages; ++p)
        for (int r = 0, first = 0; r < kb->rows_of[p]; first += kb->row[p][r++]) {
            const int width = row_quarters(kb, p, first, kb->row[p][r]) * unit / 4 + (kb->row[p][r] - 1) * kb->gap_x;
            if (width > inner) inner = width;
        }
    int key_h = pixels(key_h_t, H, height_mm);
    if (rows * key_h + (rows - 1) * kb->gap_y + 2 * kb->margin_y > H) key_h = (H - 2 * kb->margin_y - (rows - 1) * kb->gap_y) / rows;
    if (key_h < 1) return false;
    kb->unit = (int16_t)unit, kb->key_h = (int16_t)key_h, kb->rows = (uint8_t)rows;
    kb->w = (int16_t)(inner + 2 * kb->margin_x), kb->h = (int16_t)(rows * key_h + (rows - 1) * kb->gap_y + 2 * kb->margin_y);
    kb->x = (int16_t)((W - kb->w) / 2), kb->y = (int16_t)(H - kb->h);
    /* the field under it: at the top instead */
    if (field && field->w > 0 && field->h > 0 && field->x < kb->x + kb->w && kb->x < field->x + field->w && field->y < kb->y + kb->h &&
        kb->y < field->y + field->h)
        kb->y = 0;
    return true;
}

/* the keys of a page where they are: the rows in the middle of the band, each key as wide as its quarters */
static void lay(dgx_di_keyboard_t *kb, int p)
{
    const int inner = kb->w - 2 * kb->margin_x, step = kb->key_h + kb->gap_y;
    const int top   = kb->y + kb->margin_y + (kb->rows - kb->rows_of[p]) * step / 2;
    int       k     = 0;
    for (int r = 0; r < kb->rows_of[p]; ++r) {
        const int n = kb->row[p][r], width = row_quarters(kb, p, k, n) * kb->unit / 4 + (n - 1) * kb->gap_x;
        const int x0 = kb->x + kb->margin_x + (inner - width) / 2;
        int       q  = 0;
        for (int i = 0; i < n; ++i, ++k) {
            const int left = x0 + q * kb->unit / 4 + i * kb->gap_x;
            q += quarters(kb, p, k);
            const int right = x0 + q * kb->unit / 4 + i * kb->gap_x;
            kb->keys[k] = (dgx_di_rect_t){.x = (int16_t)left, .y = (int16_t)(top + r * step), .w = (int16_t)(right - left), .h = kb->key_h};
        }
    }
    kb->page = (uint8_t)p, kb->count = (uint8_t)k, kb->repeat_ms = 0;
    memset(kb->drawn, 0, sizeof kb->drawn);
    dgx_di_rects_init(&kb->rects, kb->keys, k);
}

/* a to b, t of n of the way, each of the three colors */
static uint16_t mix(uint16_t a, uint16_t b, int t, int n)
{
    const int ar = a >> 11, ag = (a >> 5) & 63, ab = a & 31, br = b >> 11, bg = (b >> 5) & 63, bb = b & 31;
    return (uint16_t)((ar + (br - ar) * t / n) << 11 | (ag + (bg - ag) * t / n) << 5 | (ab + (bb - ab) * t / n));
}

static void rounded(dgx_screen_t *s, int x, int y, int w, int h, int r, uint16_t c)
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

static void utf8(uint32_t c, char out[5])
{
    int n = 0;
    if (c < 0x80) out[n++] = (char)c;
    else if (c < 0x800) out[n++] = (char)(0xc0 | c >> 6), out[n++] = (char)(0x80 | (c & 0x3f));
    else if (c < 0x10000) out[n++] = (char)(0xe0 | c >> 12), out[n++] = (char)(0x80 | ((c >> 6) & 0x3f)), out[n++] = (char)(0x80 | (c & 0x3f));
    else
        out[n++] = (char)(0xf0 | c >> 18), out[n++] = (char)(0x80 | ((c >> 12) & 0x3f)), out[n++] = (char)(0x80 | ((c >> 6) & 0x3f)),
        out[n++] = (char)(0x80 | (c & 0x3f));
    out[n] = 0;
}

/* the signs of the keys of editing and shift, drawn: no font needs them */
static void sign(dgx_screen_t *s, uint32_t code, int cx, int cy, int size, uint16_t c)
{
    const int a = size / 4 > 2 ? size / 4 : 2, t = size / 16 > 2 ? size / 16 : 2;
    switch (code) {
    case DGX_DI_KEY_ERASE: /* an arrow to the left */
        dgx_draw_triangle_solid(s, cx - a, cy, cx - a / 4, cy - a * 3 / 4, cx - a / 4, cy + a * 3 / 4, c);
        dgx_fill_rectangle(s, cx - a / 4, cy - t / 2, a + a / 4, t, c);
        break;
    case DGX_DI_KEY_DONE: /* a bent arrow: down, then to the left */
        dgx_fill_rectangle(s, cx + a - t, cy - a, t, a + t / 2, c);
        dgx_fill_rectangle(s, cx - a / 2, cy - t / 2, a + a / 2, t, c);
        dgx_draw_triangle_solid(s, cx - a, cy, cx - a / 2, cy - a / 2, cx - a / 2, cy + a / 2, c);
        break;
    case DGX_DI_KEY_CANCEL: /* a cross */
        dgx_draw_line_thick(s, cx - a * 3 / 4, cy - a * 3 / 4, cx + a * 3 / 4, cy + a * 3 / 4, t, c);
        dgx_draw_line_thick(s, cx - a * 3 / 4, cy + a * 3 / 4, cx + a * 3 / 4, cy - a * 3 / 4, t, c);
        break;
    case DGX_DI_KEY_SHIFT: /* an arrow up */
        dgx_draw_triangle_solid(s, cx, cy - a, cx - a, cy, cx + a, cy, c);
        dgx_fill_rectangle(s, cx - a / 2, cy, a + 1, a * 3 / 4, c);
        break;
    }
}

static void draw_key(dgx_di_keyboard_t *kb, int k, bool pressed, int baseline)
{
    const layout_key_t         key = key_of(kb, kb->page, k);
    const dgx_di_rect_t *r   = &kb->keys[k];
    /* shift on: as a key of characters, the way back */
    const bool     plain = !special(key.code) || (key.code == DGX_DI_KEY_SHIFT && kb->shift);
    const uint16_t back  = kb->color[pressed ? DGX_DI_KEYBOARD_PRESSED : plain ? DGX_DI_KEYBOARD_KEY : DGX_DI_KEYBOARD_SPECIAL];
    rounded(kb->display, r->x, r->y, r->w, r->h, kb->radius, back);
    char        text[5];
    const char *label = key.label;
    if (!label && !special(key.code) && key.code != ' ') utf8(kb->shift && key.upper ? key.upper : key.code, text), label = text;
    if (label && kb->font) {
        int16_t   top, height;
        const int width = dgx_font_string_bounds(label, kb->font, &top, &height);
        dgx_font_string_utf8_screen(kb->display, (int16_t)(r->x + r->w / 2 - width / 2), (int16_t)(r->y + baseline), label, kb->color[DGX_DI_KEYBOARD_LABEL],
                                    DgxOutputNormal, 1, kb->font, NULL, NULL);
    } else if (!key.label)
        sign(kb->display, key.code, r->x + r->w / 2, r->y + r->h / 2, r->h, kb->color[DGX_DI_KEYBOARD_LABEL]);
    kb->drawn[k] = pressed;
}

/* the baseline of the labels from the top of a key: a capital in its middle */
static int baseline(const dgx_di_keyboard_t *kb)
{
    int16_t top = 0, height = 0;
    if (kb->font) dgx_font_string_bounds("H", kb->font, &top, &height);
    return kb->key_h / 2 - top - height / 2;
}

void dgx_di_keyboard_draw(dgx_di_keyboard_t *kb)
{
    if (!kb->open) return;
    dgx_screen_t  *s = kb->display;
    const uint16_t top = kb->color[DGX_DI_KEYBOARD_BACK_TOP], bottom = kb->color[DGX_DI_KEYBOARD_BACK_BOTTOM];
    dgx_screen_progress_up(s);
    if (top == bottom) dgx_fill_rectangle(s, kb->x, kb->y, kb->w, kb->h, top);
    else
        for (int i = 0; i < kb->h; ++i) dgx_fill_rectangle(s, kb->x, kb->y + i, kb->w, 1, mix(top, bottom, i, kb->h > 1 ? kb->h - 1 : 1));
    const int b = baseline(kb);
    for (int k = 0; k < kb->count; ++k) draw_key(kb, k, false, b);
    dgx_screen_progress_down(s);
}

bool dgx_di_keyboard_open(dgx_di_keyboard_t *kb, dgx_screen_t *display, int width_mm, int height_mm, const uint8_t *layout, size_t size,
                          dgx_font_t *font, const dgx_di_rect_t *field)
{
    memset(kb, 0, sizeof *kb);
    if (!display || display->color_bits != 16 || width_mm <= 0 || height_mm <= 0 || !parse(kb, layout, size)) return false;
    kb->display = display, kb->font = font;
    if (!place(kb, width_mm, height_mm, field)) return false;
    kb->open = true;
    lay(kb, 0);
    dgx_di_keyboard_draw(kb);
    return true;
}

void dgx_di_keyboard_close(dgx_di_keyboard_t *kb)
{
    kb->open = false, kb->queued = 0;
}

static void push(dgx_di_keyboard_t *kb, uint32_t code)
{
    if (kb->queued < DGX_DI_KEYBOARD_QUEUE) kb->queue[(kb->head + kb->queued++) % DGX_DI_KEYBOARD_QUEUE] = code;
}

int dgx_di_keyboard_take(dgx_di_keyboard_t *kb, dgx_di_pointer_t *p, uint32_t now_ms, uint32_t *typed, int max)
{
    if (kb->open) {
        /* the keys first (the rectangles take no finger claimed), then the fingers down on the band are the keyboard's */
        int8_t of[DGX_DI_FINGERS];
        memcpy(of, kb->rects.of, sizeof of);
        dgx_di_rects_take(&kb->rects, p, now_ms);
        bool let_go[DGX_DI_KEYBOARD_KEYS] = {0}; /* a finger of the key gone up within it, held long or not */
        for (int i = 0; i < DGX_DI_FINGERS; ++i) {
            const dgx_di_finger_t *f = &p->fingers[i];
            if (f->pressed && !f->claimed && f->x >= kb->x && f->x < kb->x + kb->w && f->y >= kb->y && f->y < kb->y + kb->h) dgx_di_pointer_claim(p, i);
            if (f->released && of[i] >= 0 && of[i] < kb->count) {
                const dgx_di_rect_t *r = &kb->keys[of[i]];
                if (f->x >= r->x && f->x < r->x + r->w && f->y >= r->y && f->y < r->y + r->h) let_go[of[i]] = true;
            }
        }
        bool all = false, turned = false; /* all drawn again; another page: the flags of this one are no more */
        for (int k = 0; k < kb->count && !turned; ++k) {
            const layout_key_t  key  = key_of(kb, kb->page, k);
            const uint8_t f    = kb->keys[k].flags;
            const bool    held = (f & DGX_DI_RECT_HELD) && (f & DGX_DI_RECT_INSIDE);
            if (key.code == DGX_DI_KEY_ERASE) {
                /* held: again and again, from a long press on */
                if (f & DGX_DI_RECT_LONG) push(kb, key.code), kb->repeat_ms = now_ms + 100;
                else if (held && kb->repeat_ms && (int32_t)(now_ms - kb->repeat_ms) >= 0) push(kb, key.code), kb->repeat_ms = now_ms + 100;
                if (!held) kb->repeat_ms = 0;
            }
            /* a key types let go inside it however long held (a rectangle's long press takes its click away: not a key's);
               erase, held, has typed again and again instead */
            if (!(key.code == DGX_DI_KEY_ERASE ? f & DGX_DI_RECT_CLICKED : let_go[k])) continue;
            if (key.code == DGX_DI_KEY_SHIFT) kb->shift = !kb->shift, all = true;
            else if (key.code >= DGX_DI_KEY_PAGE) {
                if (key.code - DGX_DI_KEY_PAGE < kb->pages) lay(kb, (int)(key.code - DGX_DI_KEY_PAGE)), kb->shift = false, all = turned = true;
            } else if (key.code < DGX_DI_KEY_SHIFT) {
                push(kb, kb->shift && key.upper ? key.upper : key.code);
                if (kb->shift && key.code >= 0x20) kb->shift = false, all = true;
            }
        }
        if (all) dgx_di_keyboard_draw(kb);
        else {
            /* the keys that changed, in one batch */
            const int b = baseline(kb);
            dgx_screen_progress_up(kb->display);
            for (int k = 0; k < kb->count; ++k) {
                const uint8_t f       = kb->keys[k].flags;
                const bool    pressed = (f & DGX_DI_RECT_HELD) && (f & DGX_DI_RECT_INSIDE);
                if (pressed != (bool)kb->drawn[k]) draw_key(kb, k, pressed, b);
            }
            dgx_screen_progress_down(kb->display);
        }
    }
    int n = 0;
    for (; n < max && kb->queued; --kb->queued, kb->head = (uint8_t)((kb->head + 1) % DGX_DI_KEYBOARD_QUEUE)) typed[n++] = kb->queue[kb->head];
    return n;
}
