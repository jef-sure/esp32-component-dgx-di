#pragma once

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include "dgx_screen.h"

#ifdef __cplusplus
extern "C" {
#endif

/*
 * DGX-DI, display input: what was touched on the display and what came of it. A companion of DGX, of no engine or
 * window library: any program that draws on the display may take it.
 *
 * Its parts: a device — a touch panel's driver, read for the fingers on it; a pointer — the glue that makes them
 * fingers of the display; a touch — the gestures of one finger.
 *
 * All of it is called by the task that owns the display (DGX: one task draws, the others send it what to draw), once a
 * step of its loop, with the time of the step: nothing here waits, locks or keeps a task of its own. A device on the
 * bus of the display is read between calls of DGX, when the bus is free; an interrupt of a device only wakes that
 * task.
 */

/* The most fingers a pointer follows: the most a touch panel tells (a GSL3680, an FT5x06: 10). */
enum { DGX_DI_FINGERS = 10 };

/* A finger as a device sees it, in the device's own coordinates. */
typedef struct {
    int16_t  x, y;
    uint16_t size; /* how hard or how wide, as the device measures it; 0: it does not */
    uint8_t  id;   /* the same while the finger stays down; DGX_DI_NO_ID: the device keeps none */
} dgx_di_point_t;

enum { DGX_DI_NO_ID = 0xff };

struct dgx_di_device;

/* The device read now: the fingers on it, at most max of them, into points; how many (0: none, or not read). */
typedef int (*dgx_di_read_fn)(struct dgx_di_device *device, dgx_di_point_t *points, int max);

/*
 * A device as its driver makes it (the driver's own struct begins with it): how it is read, how many fingers it sees at
 * once, and its range — what it gives from one edge of the display to the other, x_min..x_max and y_min..y_max along
 * its own axes. A touch panel with a driver of ESP-IDF's esp_lcd_touch where there is one, a driver of this component
 * where there is not (dgx_di_gsl3680.h).
 */
typedef struct dgx_di_device {
    dgx_di_read_fn read;
    uint8_t        fingers; /* so many at once, as the panel sees them (0: one); a pointer follows up to DGX_DI_FINGERS */
    int16_t        x_min, x_max, y_min, y_max;
} dgx_di_device_t;

/*
 * A pointer: the glue that makes the fingers of a device fingers of the display, in pixels of it as it stands now: the
 * device's axes mirrored, then swapped (as the panel lies under the display in the display's own orientation, as its
 * controller scans it before any orientation of DGX), its range scaled to the display's, corrected by the board's
 * calibration (below), and turned as the screen of DGX is turned now (its dir_x, dir_y, swap_xy: the axes swapped, then
 * counted the other way along the panel's own). The program turns the display, the fingers turn with it.
 *
 * Down and up are steadied for each finger: down when the device has seen it so long running, up when it has not so
 * long (a sensitive panel blinks at its threshold, the finger still above the glass). A finger is known by the number
 * the device gives it; a device that keeps none gets numbers from the glue, each finger of a read taken for the
 * nearest finger of the last. A device's numbers are not taken blindly: a read with a number twice is torn and not taken
 * (the fingers as they were), and a point of one finger's number lying on another finger, far from its own, is that
 * other finger's echo (a GT911 tears a read so as a finger goes up).
 *
 * The first finger is the one that went down when none was, until it goes up; after it none is first until all are up
 * (a finger left down is no new touch). It is what a touch (below) or a program of one pointer takes. A finger claimed
 * (a pad's: it went down on a button of the pad) is no finger for that: not first, and down it keeps none from being
 * first — the program's touch is its own, the pad's fingers the pad's.
 *
 * RAM: the struct, 308 bytes on a 32-bit chip (28 of them a finger, room for DGX_DI_FINGERS); the calibration is read
 * where it lies (flash, or wherever the program keeps it).
 */

enum {
    DGX_DI_MIRROR_X = 1, /* the device's x counted the other way */
    DGX_DI_MIRROR_Y = 2, /* its y */
    DGX_DI_SWAP_XY  = 4, /* then its x along the display's y and its y along x */
};

/* A point of a calibration: where on the display a touch is taken, where it was meant (pixels of one axis). */
typedef struct {
    int16_t taken, meant;
} dgx_di_knot_t;

/*
 * A calibration: how a board's touch panel lies under its display, and how far its touches are taken from where they
 * were meant, measured once (dgx_di_calibrate.h) and kept — pasted into the program (dgx_di_calibration_code) or kept
 * in NVS. Flat, no pointers. In pixels of the display in its own orientation: the device's point mirrored and swapped
 * by flags and scaled from its range is corrected by the affine (a panel skewed, turned, stretched), then by the points
 * of each axis (what bends: an edge pressed together); between two points in proportion, past the ends as the end's
 * piece goes, within the display.
 */
enum { DGX_DI_CALIBRATION_VERSION = 1, DGX_DI_KNOTS = 3 };

typedef struct {
    uint16_t      version;                    /* DGX_DI_CALIBRATION_VERSION */
    uint16_t      size;                       /* sizeof(dgx_di_calibration_t) */
    int16_t       x_min, x_max, y_min, y_max; /* the range of the device it is for */
    int16_t       width, height;              /* the display's it is for, in its own orientation */
    uint8_t       flags;                      /* DGX_DI_*: how the device lies under the display */
    uint8_t       knots_x, knots_y;           /* so many points of each axis below (0: none) */
    uint8_t       rounds;                     /* measured over so many rounds of targets */
    int32_t       affine[6];                  /* x' = (a x + b y + c) / 65536, y' = (d x + e y + f) / 65536; all 0: none */
    dgx_di_knot_t x[DGX_DI_KNOTS], y[DGX_DI_KNOTS]; /* then where taken against where meant, by taken in order */
    uint16_t      down_ms, up_ms;             /* how to steady it, for how it blinked: */
    uint16_t      blink_ms, gap_ms;           /* the longest it showed a finger near the glass, lost one held */
    uint16_t      off_tenths, worst_tenths;   /* how far off it took the targets without it: on average, at worst */
    uint16_t      left_tenths, left_worst_tenths; /* and with it (tenths of a pixel) */
} dgx_di_calibration_t;

/* A target of a calibration: where it was meant, in pixels of the display in its own orientation; where the device took it. */
typedef struct {
    int16_t meant_x, meant_y;
    int16_t x, y; /* the device's own coordinates, the touches on it on average */
} dgx_di_aim_t;

/* A finger of the display. */
typedef struct {
    int16_t  x, y;              /* where down, in pixels of the display as it stands (where it went up when not down);
                                   at the read it went down, where the device first saw it: steadying takes time, the
                                   finger moves meanwhile (a swipe from an edge leaves the edge) */
    uint16_t size;              /* as the device measures it (0: it does not) */
    uint8_t  id;                /* its number while it is down: the device's, or the glue's for a device that keeps none */
    bool     down;              /* as last read, steadied */
    bool     pressed, released; /* went down, went up at the last read */
    bool     sees;              /* the device saw it at the last read (down or not yet, up or not yet) */
    bool     claimed;           /* taken by a part of DGX-DI (a pad's button) until it goes up: never the first finger */
    int16_t  seen_x, seen_y;    /* where the device saw it last, in pixels of the display */
    int16_t  taken_x, taken_y;  /* where the device saw it last, before the calibration and the turn (for making one) */
    int16_t  landed_x, landed_y; /* where the device began to see it, this time running */
    uint32_t since_ms;          /* when the device began to see it, or to lose it, against down as it stands */
} dgx_di_finger_t;

typedef struct {
    dgx_di_device_t     *device;
    const dgx_screen_t  *screen;          /* the display: its size and orientation as they are now */
    uint8_t              flags;           /* DGX_DI_*: how the device lies under the display in its own orientation */
    const dgx_di_calibration_t *calibration; /* NULL: none */
    uint16_t             down_ms, up_ms;  /* seen so long running makes a finger down, lost so long makes it up */
    int8_t               first;           /* the first finger, an index of fingers; -1: none */
    bool                 first_up;        /* the first went up, others stay: none is first until all are up */
    int16_t              first_x, first_y; /* where the first finger was last */
    uint8_t              next_id;         /* the glue's next number, for a device that keeps none */
    dgx_di_finger_t      fingers[DGX_DI_FINGERS]; /* in use: down, seen or just released */
} dgx_di_pointer_t;

/* A pointer of a device under a display (a screen of DGX), the device's axes as flags say. */
void dgx_di_pointer_init(dgx_di_pointer_t *pointer, dgx_di_device_t *device, int flags, const dgx_screen_t *screen);

/*
 * The board's calibration taken (NULL: none): true when it is for this device and this display (their range and size),
 * the pointer then lying by its flags, steadied by its times and corrected by it; false and nothing changed when not —
 * measure the board again. Kept, not copied.
 */
bool dgx_di_pointer_calibrate(dgx_di_pointer_t *pointer, const dgx_di_calibration_t *calibration);

/*
 * A calibration made from targets measured on a device under a display of width x height (its own orientation): the
 * flags found (which of the device's axes goes along which of the display's, and which way), the affine of least
 * squares over all the targets, then the points of each axis through what is left (the targets by where they were
 * meant along it, up to DGX_DI_KNOTS places); how far off without and with it. false when the targets do not span the
 * display (fewer than 4, or all in a line).
 */
bool dgx_di_calibration_make(dgx_di_calibration_t *calibration, const dgx_di_device_t *device, int width, int height,
                             const dgx_di_aim_t *aims, int count);

/*
 * How to steady the panel, from how it blinked: down after the longest blink and a little more, up after the longest
 * gap and a little more, 30 ms at least (a measure may see no gap the panel has at another time; two quick taps of a key
 * lift for 40 ms between them).
 */
void dgx_di_calibration_steady(dgx_di_calibration_t *calibration, int blink_ms, int gap_ms);

/*
 * What a calibration says about the board, in words, for whoever reads them: a log, a screen, a report of a driver that
 * does not work. English, one paragraph, under 400 characters. As snprintf: the length of the whole text.
 */
int dgx_di_calibration_text(const dgx_di_calibration_t *calibration, char *text, size_t size);

/* The calibration as C, an initializer of a dgx_di_calibration_t to paste into the program. As snprintf. */
int dgx_di_calibration_code(const dgx_di_calibration_t *calibration, char *text, size_t size);

/* Down after the device has seen a finger down_ms running, up after it has not seen it up_ms (0, 0: as it says). */
void dgx_di_pointer_debounce(dgx_di_pointer_t *pointer, int down_ms, int up_ms);

/*
 * A point of the device as a point of the display as it stands now (within it); taken: there in the display's own
 * orientation, before the calibration (may be NULL).
 */
void dgx_di_pointer_map(const dgx_di_pointer_t *pointer, int dx, int dy, int *x, int *y, int16_t taken[2]);

/* The device read at now_ms: its fingers steadied, as fingers of the display (pointer->fingers); how many are down. */
int dgx_di_pointer_read(dgx_di_pointer_t *pointer, uint32_t now_ms);

/* The first finger, as last read: true when down, *x and *y where (where it, or the last first one, went up when not). */
bool dgx_di_pointer_first(const dgx_di_pointer_t *pointer, int *x, int *y);

/* The finger of that index claimed till it goes up (a pad: dgx_di_pad.h), after the read it went down at: not first. */
void dgx_di_pointer_claim(dgx_di_pointer_t *pointer, int finger);

/* The edges of the display a touch may be swiped from (a panel drawn down from the top, as on a phone). */
enum {
    DGX_DI_EDGE_TOP    = 1u << 0,
    DGX_DI_EDGE_BOTTOM = 1u << 1,
    DGX_DI_EDGE_LEFT   = 1u << 2,
    DGX_DI_EDGE_RIGHT  = 1u << 3,
};
/* The ways a touch may be swiped. */
enum {
    DGX_DI_SWIPE_UP    = 1u << 0,
    DGX_DI_SWIPE_DOWN  = 1u << 1,
    DGX_DI_SWIPE_LEFT  = 1u << 2,
    DGX_DI_SWIPE_RIGHT = 1u << 3,
};
/*
 * The gestures, in millimetres of the display and milliseconds: a swipe, away from where it went down TRAVEL along an
 * axis; from an edge, when it went down within BAND of that edge and went away from it; a long press, down LONG and
 * never farther than SLOP from where it went down.
 */
enum { DGX_DI_EDGE_BAND_MM = 9, DGX_DI_SWIPE_TRAVEL_MM = 15, DGX_DI_LONG_MS = 500, DGX_DI_LONG_SLOP_MM = 3 };

/*
 * A touch: what a finger does on the display, from its points in pixels of the display, whoever gives them (the first
 * finger of a pointer, a mouse, a script): where it is, down now, went down, went up since the last look; one gesture a
 * touch, told once as it comes — a swipe from an edge, the edge (it went away from it: that, not a plain swipe); else a
 * plain swipe, the way it went (both axes far enough: the one gone the larger share of its way); or a long press, but
 * not by an edge (it may yet be a swipe from it: a finger rests at the edge before it sets out). No gestures but down and
 * up until the display is given (dgx_di_touch_display). RAM: 32 bytes on a 32-bit chip.
 */
typedef struct {
    const dgx_screen_t *screen;    /* the display, for the gestures (its size as it stands now); NULL: none */
    int16_t             mm_w, mm_h; /* its size in millimetres, in its own orientation (as its controller scans it) */
    int16_t             x, y;      /* where, in pixels of the display (where it went up when not down) */
    bool                down, pressed, released;
    bool                long_press; /* held long since the last look */
    uint8_t             edge;      /* DGX_DI_EDGE_*: swiped from that edge since the last look */
    uint8_t             swipe;     /* DGX_DI_SWIPE_*: swiped that way since the last look */
    uint8_t             from;      /* the edges it went down at, while it may still be a swipe from one of them */
    bool                told;      /* this touch has told its gesture */
    bool                strayed;   /* gone farther than the slop of a long press */
    int16_t             from_x, from_y; /* where it went down */
    uint32_t            from_ms;   /* when */
} dgx_di_touch_t;

/*
 * The display the touch is on, and its size in millimetres, measured — the developer's, DGX does not know it: its width
 * and height as it stands now (as the screen gives its pixels now); they turn with it after. NULL or 0: no gestures.
 */
void dgx_di_touch_display(dgx_di_touch_t *touch, const dgx_screen_t *screen, int width_mm, int height_mm);

/* Take the point as it is at now_ms: where, and whether it is down; pressed, released and a gesture are worked out from the last time. */
void dgx_di_touch_take(dgx_di_touch_t *touch, int x, int y, bool down, uint32_t now_ms);

/*
 * Rectangles: places of the display a finger presses — buttons the program draws itself. The program gives an array of
 * them in pixels of the display as it stands; each look goes over the fingers of a pointer and sets the flags of each
 * rectangle. A finger is the rectangle's it went down in (the first of the array it is in: they should not overlap),
 * until it goes up, wherever it is meanwhile: no blinking at the border, and a finger led out and let go is told from
 * one let go inside (a click). A long press is told as the touch's (DGX_DI_LONG_MS), when the finger has stayed inside
 * all the time; the finger let go after it is no click (as on a phone). A finger that went down beside all of them
 * holds none, though it goes over them after; nor does a finger claimed (a pad's: dgx_di_pad_take before this). Several
 * fingers: the flags of several rectangles at once, of one rectangle from any finger of it. No layer, no mask, no
 * PSRAM: a resistive panel of one finger does too.
 */
enum {
    DGX_DI_RECT_HELD     = 1u << 0, /* a finger of it is down (wherever it is now) */
    DGX_DI_RECT_INSIDE   = 1u << 1, /* and inside it now (out: let go there, the press is no click) */
    DGX_DI_RECT_PRESSED  = 1u << 2, /* a finger went down in it since the last look */
    DGX_DI_RECT_RELEASED = 1u << 3, /* a finger of it went up since the last look, wherever */
    DGX_DI_RECT_CLICKED  = 1u << 4, /* and inside it, no long press before: a click */
    DGX_DI_RECT_LONG     = 1u << 5, /* held long inside it, told once a touch */
    DGX_DI_RECT_FORCED   = 1u << 6, /* pressed hard: for a panel that measures it; none sets it yet */
};

typedef struct {
    int16_t x, y, w, h; /* given: in pixels of the display as it stands */
    uint8_t flags;      /* DGX_DI_RECT_*, as last taken */
    uint8_t fingers;    /* so many fingers of it down */
} dgx_di_rect_t;

/* The rectangles of a program and whose the fingers of a pointer are. RAM: 80 bytes on a 32-bit chip, 7 of them a finger. */
typedef struct {
    dgx_di_rect_t *rects;
    int            count;
    int8_t         of[DGX_DI_FINGERS];       /* the rectangle each finger of the pointer is; -1: none */
    bool           strayed[DGX_DI_FINGERS];  /* gone out of it since it went down: no long press */
    bool           told[DGX_DI_FINGERS];     /* its long press told */
    uint32_t       since_ms[DGX_DI_FINGERS]; /* when it went down */
} dgx_di_rects_t;

/* Rectangles of the program to go over (taken again: the fingers of them let go). The array is kept, not copied. */
void dgx_di_rects_init(dgx_di_rects_t *rects, dgx_di_rect_t *array, int count);

/*
 * The fingers of the pointer as last read (dgx_di_pointer_read) taken at now_ms: the flags of each rectangle set; the
 * rectangle a finger went down in at this look (several: the first of them in the array), -1: none (no finger went
 * down, or beside all).
 */
int dgx_di_rects_take(dgx_di_rects_t *rects, const dgx_di_pointer_t *pointer, uint32_t now_ms);

#ifdef __cplusplus
}
#endif
