#pragma once

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include "dgx_di.h"
#include "dgx_font.h"
#include "dgx_screen.h"

#ifdef __cplusplus
extern "C" {
#endif

/*
 * A keyboard on the display: keys drawn into a band of the display, opaque, and out of the fingers on them a stream of
 * code points, as a keyboard of wires gives it: characters, U+0008 to erase, U+000A done, U+001B cancel.
 *
 * It draws straight on the display, no layer: while it is open, the band is its own (kb.x, y, w, h) and the program
 * draws nothing there; dgx_di_keyboard_draw() draws it all again after the program has drawn over it. Each drawing is
 * one batch of DGX. Its keys are rectangles of dgx_di.h: a finger is the key's it went down on, the key types when it is
 * let go inside it, however long held (led out and let go: nothing, as on a phone), a key pressed is drawn so; several
 * fingers type over one another. A finger that goes down on the band is the keyboard's (dgx_di_pointer_claim): never
 * the program's first finger nor a rectangle of its own. Erase held types again and again: from a long press on, every
 * 100 ms, and nothing more when let go.
 *
 * Its layout is data: the JSON of tools/keyboard-editor made a blob by tools/keyboard_blob.py (the format is told
 * there): pages, rows of keys on each, a key what it types, what with shift, its label, its width; its sizes in tenths
 * of a millimetre and its colors. A key is as wide as the layout says, but no wider than lets the longest row of all
 * its pages into the display; the rows in the middle; the band at the bottom of the display in the middle (at the top
 * when it would cover the field the program gives). The signs of erase, done, cancel and shift are drawn, not taken
 * from the font. A 16-bit display (RGB565). Called from the task that owns the display, as the rest of DGX-DI; not
 * shown together with a pad (dgx_di_pad.h).
 *
 * RAM: the struct, some 0.8 KB on a 32-bit chip; the blob is read where it lies (flash). No PSRAM, no layer.
 */

enum {
    DGX_DI_KEYBOARD_PAGES = 4,  /* the most pages a layout has */
    DGX_DI_KEYBOARD_ROWS  = 5,  /* the most rows a page has */
    DGX_DI_KEYBOARD_KEYS  = 48, /* the most keys a page has */
    DGX_DI_KEYBOARD_QUEUE = 16, /* code points typed and not yet taken, at most */
};

/* The code points of editing a keyboard types; its keys of them are drawn with their signs. */
enum { DGX_DI_KEY_ERASE = 0x08, DGX_DI_KEY_DONE = 0x0a, DGX_DI_KEY_CANCEL = 0x1b };

/* What a key of a layout does besides typing (past the last code point of Unicode): never typed. */
#define DGX_DI_KEY_SHIFT 0x110000u /* the next character as with shift (upper case) */
#define DGX_DI_KEY_PAGE  0x110100u /* + n: to page n of the layout */

/* The ready layouts, blobs: a code (1 to 9, erase, 0, done); Latin letters with shift, and a page of digits and signs. */
extern const uint8_t dgx_di_keyboard_digits[], dgx_di_keyboard_latin[];
extern const size_t  dgx_di_keyboard_digits_size, dgx_di_keyboard_latin_size;

/* The colors of a layout, in order. */
enum { DGX_DI_KEYBOARD_BACK_TOP, DGX_DI_KEYBOARD_BACK_BOTTOM, DGX_DI_KEYBOARD_KEY, DGX_DI_KEYBOARD_SPECIAL, DGX_DI_KEYBOARD_PRESSED, DGX_DI_KEYBOARD_LABEL, DGX_DI_KEYBOARD_COLORS };

typedef struct {
    const uint8_t *layout; /* the blob, kept */
    dgx_screen_t  *display;
    dgx_font_t    *font;
    bool           open;
    bool           shift; /* the next character as with shift */
    uint8_t        page, pages;
    uint8_t        rows_of[DGX_DI_KEYBOARD_PAGES];                     /* the rows of each page */
    uint8_t        row[DGX_DI_KEYBOARD_PAGES][DGX_DI_KEYBOARD_ROWS];   /* so many keys in each */
    uint16_t       first[DGX_DI_KEYBOARD_PAGES];                       /* its first key among all */
    uint16_t       color[DGX_DI_KEYBOARD_COLORS];
    int16_t        x, y, w, h;                                         /* its band of the display */
    int16_t        unit, gap_x, gap_y, key_h, margin_x, margin_y, radius; /* in pixels */
    uint8_t        rows;                                               /* of the band: the most of all pages */
    uint8_t        count;                                              /* the keys of the page */
    dgx_di_rect_t  keys[DGX_DI_KEYBOARD_KEYS];                         /* where, in pixels of the display */
    uint8_t        drawn[DGX_DI_KEYBOARD_KEYS];                        /* as each was last drawn: 1 pressed */
    dgx_di_rects_t rects;
    uint32_t       repeat_ms; /* the next erase of the erase key held; 0: none */
    uint32_t       queue[DGX_DI_KEYBOARD_QUEUE];
    uint8_t        head, queued;
} dgx_di_keyboard_t;

/*
 * The keyboard of a layout (a blob of size bytes, kept, not copied) opened on a display as it stands, measured
 * width_mm x height_mm, its labels in a font (NULL: none but the signs): placed (away from field, the rectangle of the
 * program's field in pixels of the display; NULL: none) and drawn. false, nothing drawn: no such blob, a layout past the
 * limits, or one that does not fit the display, or the display not 16-bit.
 */
bool dgx_di_keyboard_open(dgx_di_keyboard_t *kb, dgx_screen_t *display, int width_mm, int height_mm, const uint8_t *layout, size_t size,
                          dgx_font_t *font, const dgx_di_rect_t *field);

/* Closed: nothing more drawn or taken; its band the program's again, to draw over. */
void dgx_di_keyboard_close(dgx_di_keyboard_t *kb);

/* All of it drawn again (the program drew over its band). */
void dgx_di_keyboard_draw(dgx_di_keyboard_t *kb);

/*
 * The fingers of the pointer as last read taken at now_ms: those gone down on the band claimed, the keys pressed drawn
 * so, the keys let go typed. The code points typed since the last take into typed, at most max (the rest stay for the
 * next); how many. Before the program's first finger is asked, as a pad's take.
 */
int dgx_di_keyboard_take(dgx_di_keyboard_t *kb, dgx_di_pointer_t *pointer, uint32_t now_ms, uint32_t *typed, int max);

#ifdef __cplusplus
}
#endif
