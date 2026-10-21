#pragma once

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include "dgx_di.h"
#include "dgx_screen.h"

#ifdef __cplusplus
extern "C" {
#endif

/*
 * A pad on the display: buttons, a cross and sticks drawn translucent over the picture of a game, played with the
 * thumbs; out of it the buttons and axes of a game pad, as a pad of wires gives them.
 *
 * Its buttons are the bits of DGX-GE's dgx_ge_input.h one for one — the standard layout of a game pad (SDL's
 * GameController, the W3C Gamepad API) in the engine's order, its first seven where they were; its axes the engine's,
 * whole travel, no dead zone (the engine takes that off).
 *
 * Its items are given in millimetres from an edge or the middle of the display (a cross under the left thumb, the
 * buttons under the right), and made pixels of the display as it stands when the pad is made. It draws them into a
 * layer of DGX (dgx_overlay.h, DGX 0.6.6): an 8-bit virtual screen of the whole display, mixed with what goes to the
 * display from virtual screens; beside it a mask of the same size, the number of the item each pixel is: a finger is
 * one read of it. A finger that goes down on an item is the pad's until it goes up (dgx_di_pointer_claim: never the
 * program's first finger): a button held while the finger stays within half its radius again from its middle, a cross
 * as it is drawn (on an arm that way, in a corner between two arms both), a stick by how far. An item pressed is drawn so, and the layer marks its
 * place for the owner of the display to send again (dgx_overlay_take_dirty). Called from the task that owns the
 * display, as the rest of DGX-DI.
 *
 * Its items are C (dgx_di_pad_item_t, drawn as by default: a button a circle with a rim, a sign on START, BACK, HOME
 * and a chevron on a way; a cross as the flat skin of Delta has one, a plus with a light rim on a round plate, darker,
 * a chevron on each arm and a bracket in each corner, white when held; a stick a well with a knob) or the
 * layout of a blob (dgx_di_pad_open): the JSON of tools/pad-editor made a blob by tools/pad_blob.py, its items with
 * pictures of their own — shapes in tenths of a millimetre from an item's middle (circles, rings, rectangles,
 * polygons, lines of points), drawn by DGX in the pixels of this display: a button's normal and pressed; a cross's
 * normal, an arm held of each way and a corner held of each diagonal (over it); a stick's well, knob and knob held. The
 * colors of the layer are those the items and their pictures use (up to 255). A finger takes an item by its circle of
 * its radius, whatever is drawn.
 *
 * Memory: the layer and the mask, a byte a pixel of the display each (2 MB on 1280x800: PSRAM), 24 bytes an item and
 * 512 of its colors; a blob's items besides (20 bytes each), its pictures read where they lie.
 */

/* The buttons, the engine's bits. */
enum {
    DGX_DI_PAD_LEFT  = 1u << 0,
    DGX_DI_PAD_RIGHT = 1u << 1,
    DGX_DI_PAD_UP    = 1u << 2,
    DGX_DI_PAD_DOWN  = 1u << 3,
    DGX_DI_PAD_A     = 1u << 4,
    DGX_DI_PAD_START = 1u << 5,
    DGX_DI_PAD_BACK  = 1u << 6,
    DGX_DI_PAD_B     = 1u << 7,
    DGX_DI_PAD_X     = 1u << 8,
    DGX_DI_PAD_Y     = 1u << 9,
    DGX_DI_PAD_HOME  = 1u << 10,
    DGX_DI_PAD_LB    = 1u << 11,
    DGX_DI_PAD_RB    = 1u << 12,
    DGX_DI_PAD_LT    = 1u << 13,
    DGX_DI_PAD_RT    = 1u << 14,
    DGX_DI_PAD_L3    = 1u << 15,
    DGX_DI_PAD_R3    = 1u << 16,
};

/* The axes, the engine's: sticks -DGX_DI_AXIS_MAX..DGX_DI_AXIS_MAX (x to the right, y down), triggers 0..max. */
enum { DGX_DI_AXIS_LX, DGX_DI_AXIS_LY, DGX_DI_AXIS_RX, DGX_DI_AXIS_RY, DGX_DI_AXIS_LT, DGX_DI_AXIS_RT, DGX_DI_AXES };
#define DGX_DI_AXIS_MAX 32767

/* The kinds of items. */
enum {
    DGX_DI_PAD_BUTTON, /* a round button: its bit while held */
    DGX_DI_PAD_CROSS,  /* a cross: the arrows by the way the finger lies from its middle */
    DGX_DI_PAD_STICK,  /* a stick: two axes by how far the finger is from its middle, its radius the whole travel */
};

/* What an item's place is measured from, each axis: an edge (the left, the top), the middle, the other edge. */
enum { DGX_DI_FROM_START, DGX_DI_FROM_MIDDLE, DGX_DI_FROM_END };

/* The ways of an item. */
enum {
    DGX_DI_PAD_FOUR     = 1, /* a cross of four ways only (else as drawn: an arm its way, a corner between two both) */
    DGX_DI_PAD_FLOATING = 2, /* a stick's middle where the finger went down, not its own */
};

/*
 * An item of a pad: its place from where from_x and from_y say, toward the middle of the display (from the right or the
 * bottom: leftward, upward; from the middle: right and down, so may be less than 0), and its radius, tenths of a
 * millimetre; its button (a bit) or, a stick, 0 left and 1 right; its color, RGB565 (0: grey).
 */
typedef struct {
    uint8_t  kind;
    uint8_t  from_x, from_y;
    uint8_t  ways;
    int16_t  x, y, r;
    uint32_t out;
    uint16_t color;
} dgx_di_pad_item_t;

/* The pad of a game console for tablets: a cross under the left thumb, A B X Y under the right, BACK and START between. */
extern const dgx_di_pad_item_t dgx_di_pad_classic[];
#define DGX_DI_PAD_CLASSIC 7

/*
 * The ready layouts as blobs (dgx_di_pad_open): the classic one as it is above; the classic flat, its pictures of
 * plain shapes; twin, a floating stick under the left thumb, A B X Y, LB and RB at the top corners, BACK and START, of
 * glass.
 */
extern const uint8_t dgx_di_pad_layout_classic[], dgx_di_pad_layout_flat[], dgx_di_pad_layout_twin[];
extern const size_t  dgx_di_pad_layout_classic_size, dgx_di_pad_layout_flat_size, dgx_di_pad_layout_twin_size;

/* An item in pixels of the display, and what it shows. */
typedef struct {
    int16_t  x, y, r;
    uint16_t pictured; /* drawn by its pictures of the blob (else as by default) */
    uint32_t state;    /* as last taken: a button down, a cross's arrows, a stick's knob (packed, 0: at rest) */
    uint32_t drawn;    /* as last drawn */
    int16_t  box[4];   /* what it draws: left, top, right, bottom (inclusive) */
} dgx_di_pad_place_t;

typedef struct {
    const dgx_di_pad_item_t *items;
    int                      count;
    const uint8_t           *blob;    /* the layout it was opened of: its pictures; NULL: none */
    dgx_di_pad_item_t       *owned;   /* the items made of the blob */
    uint16_t                *palette; /* the colors of the layer by index, RGB565 (0: none) */
    int                      colors;  /* so many, the none among them */
    int16_t                  mm_w, mm_h; /* the display's millimetres it was made for */
    dgx_di_pad_place_t      *places;
    dgx_screen_t            *layer;                  /* what it looks like: an 8-bit virtual screen of the display */
    uint8_t                 *mask;                   /* the item each pixel is, 1..count; 0: none */
    int16_t                  width, height;          /* the display, as it stood when the pad was made */
    int8_t                   of[DGX_DI_FINGERS];     /* the item each finger of the pointer holds; -1: none */
    int16_t                  down_x[DGX_DI_FINGERS], down_y[DGX_DI_FINGERS]; /* where it went down (a floating stick) */
    uint32_t                 buttons, pressed, released; /* held now; went down, went up since the last take */
    int16_t                  axes[DGX_DI_AXES];
} dgx_di_pad_t;

/*
 * A pad of these items (kept, not copied) on a display as it stands, measured width_mm x height_mm: the layer and the
 * mask made and drawn. false: no memory, or more than 124 items.
 */
bool dgx_di_pad_init(dgx_di_pad_t *pad, const dgx_screen_t *display, int width_mm, int height_mm, const dgx_di_pad_item_t *items, int count);

/*
 * A pad of a layout (a blob of tools/pad_blob.py, size bytes, kept, not copied) on a display as it stands, measured
 * width_mm x height_mm: its items made, the layer and the mask made and drawn. false: no memory, or no such blob, or one
 * past the limits (124 items, shapes and points of an item within the blob, polygons of 3 points at least).
 */
bool dgx_di_pad_open(dgx_di_pad_t *pad, const dgx_screen_t *display, int width_mm, int height_mm, const uint8_t *layout, size_t size);

/* Its layer and mask given back (hidden first, when shown). */
void dgx_di_pad_free(dgx_di_pad_t *pad, dgx_screen_t *display);

/* Laid over the display, alpha 0..255 how much of its colors over the picture (160: well seen, the game too). */
bool dgx_di_pad_show(dgx_di_pad_t *pad, dgx_screen_t *display, uint8_t alpha);

/* Taken away from the display; its place there marked to be sent again. Another layer laid over it stays. */
void dgx_di_pad_hide(dgx_di_pad_t *pad, dgx_screen_t *display);

/* The item at x, y of the display; -1: none. */
int dgx_di_pad_item_at(const dgx_di_pad_t *pad, int x, int y);

/*
 * The fingers of the pointer as last read taken: those gone down on an item claimed, the buttons and axes made, the
 * items changed drawn again. Before the program's first finger is asked (dgx_di_pointer_first, a touch).
 */
void dgx_di_pad_take(dgx_di_pad_t *pad, dgx_di_pointer_t *pointer);

#ifdef __cplusplus
}
#endif
