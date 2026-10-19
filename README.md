# DGX-DI

DGX-DI is touch input for displays on ESP32 microcontrollers: where the
display was touched, and what came of it. It goes with my display library
[DGX](https://github.com/jef-sure/esp32-component-dgx). DGX puts the picture
on the display, and DGX-DI tells where a finger is on that picture, in the
same pixels, however the display is turned. I made it for my game engine
[DGX-GE](https://github.com/jef-sure/esp32-component-dgx-ge), but it belongs
to no engine and no window library. A program that draws a single "OK"
button by itself uses it the same way.

What it gives:

- the fingers of a touch panel as fingers of the display: up to 10 of them,
  followed from one read to the next, steadied in milliseconds, turned with
  the screen;
- a calibration measured once on the board, with targets it draws on the
  display itself, and kept in the program;
- the gestures of a touch, in millimetres: a tap, a swipe from an edge (like
  the panel of a phone), a plain swipe, a long press;
- rectangles: buttons the program draws itself, each finger held by the one
  it went down in;
- a translucent game pad over a game, in a layer of DGX: a cross, buttons
  and sticks, read as the buttons and axes of a real game pad;
- a driver of the Silead GSL3680, and any panel of ESP-IDF's `esp_lcd_touch`
  (GT911, FT5x06, CST816, XPT2046 and the rest).

This is version 0.0.1, the first one. Its interfaces may still change.

## Contents

- [Getting started](#getting-started)
- [A minimal example](#a-minimal-example)
- [Recipes](#recipes)
  - [A panel ESP-IDF has a driver for](#a-panel-esp-idf-has-a-driver-for)
  - [A GSL3680](#a-gsl3680)
  - [Your own driver](#your-own-driver)
  - [Calibrate a board once](#calibrate-a-board-once)
  - [Without a calibration](#without-a-calibration)
  - [Taps, swipes from an edge, a long press](#taps-swipes-from-an-edge-a-long-press)
  - [Several fingers](#several-fingers)
  - [Buttons the program draws](#buttons-the-program-draws)
  - [A game pad over a game](#a-game-pad-over-a-game)
  - [A pad of your own](#a-pad-of-your-own)
  - [Rotate the screen](#rotate-the-screen)
- [Boards it was tried on](#boards-it-was-tried-on)
- [How it fits together](#how-it-fits-together)
- [Coordinates and orientation](#coordinates-and-orientation)
- [API reference](#api-reference)
  - [The device](#the-device)
  - [The pointer](#the-pointer)
  - [The calibration](#the-calibration)
  - [The touch](#the-touch)
  - [The rectangles](#the-rectangles)
  - [The pad](#the-pad)
  - [Drivers](#drivers)
- [Memory](#memory)
- [Known gaps](#known-gaps)
- [Host tests](#host-tests)
- [Repository layout](#repository-layout)
- [License](#license)

## Getting started

DGX-DI is an ESP-IDF component, published in the ESP Component Registry as
`jef-sure/dgx-di`. Add it to a project:

```sh
idf.py add-dependency "jef-sure/dgx-di^0.0.1"
```

or put it into `main/idf_component.yml` yourself:

```yaml
dependencies:
  jef-sure/dgx-di: "^0.0.1"
```

It brings DGX along (`jef-sure/dgx` 0.6.6 or a later 0.6: the pad needs the
overlay layer of 0.6.6). It needs ESP-IDF 5.3 or later, and was built with
5.5.5, 6.0.3 and 6.1 for the ESP32 and the ESP32-P4.

To work on DGX-DI itself, check the repository out under the project's
`components/` instead; the component manager still brings DGX for it. A
checkout of DGX beside it must sit in `components/jef-sure__dgx`: DGX-DI
requires DGX by the name the registry gives it, and a local component of that
name takes the place of the registry's.

## A minimal example

The GT911 of the 7-inch Guition JC1060P470, on an I2C bus the board has set
up, under a display of DGX:

```c
#include "dgx_di.h"
#include "dgx_di_esp_lcd_touch.h"
#include "esp_lcd_touch_gt911.h"
#include "esp_timer.h"

static dgx_di_esp_lcd_touch_t panel;
static dgx_di_pointer_t       pointer;

void touch_start(const dgx_screen_t *screen, i2c_master_bus_handle_t i2c)
{
    // 1. the panel's own driver, its mirroring and swapping left off
    esp_lcd_panel_io_handle_t     io;
    esp_lcd_panel_io_i2c_config_t io_config = ESP_LCD_TOUCH_IO_I2C_GT911_CONFIG();
    const esp_lcd_touch_config_t  config    = {.x_max = 1024, .y_max = 600,
                                               .rst_gpio_num = GPIO_NUM_NC, .int_gpio_num = GPIO_NUM_NC};
    esp_lcd_touch_handle_t        handle;
    esp_lcd_new_panel_io_i2c(i2c, &io_config, &io);
    esp_lcd_touch_new_i2c_gt911(io, &config, &handle);

    // 2. the panel as a device of DGX-DI: 5 fingers at once, each with its own number
    dgx_di_esp_lcd_touch_init(&panel, handle, 5, true);

    // 3. the pointer: that device under this display, lying as the display does (no flags)
    dgx_di_pointer_init(&pointer, &panel.device, 0, screen);
}

// once a step of the loop of the task that draws
void touch_step(void)
{
    dgx_di_pointer_read(&pointer, (uint32_t)(esp_timer_get_time() / 1000));
    int x, y;
    if (dgx_di_pointer_first(&pointer, &x, &y)) {
        // a finger is down at x, y: pixels of the display as it stands now
    }
}
```

The project adds the panel's driver to `main/idf_component.yml`
(`espressif/esp_lcd_touch_gt911: "^1"`).

## Recipes

Short answers to the things people look for first. The functions are listed
in the [API reference](#api-reference); the headers say the rest.

### A panel ESP-IDF has a driver for

Any driver of `esp_lcd_touch` is a device of DGX-DI:

```c
#include "dgx_di_esp_lcd_touch.h"

static dgx_di_esp_lcd_touch_t panel;

dgx_di_esp_lcd_touch_init(&panel, handle, /* fingers at once */ 5, /* its numbers stay with a finger */ true);
dgx_di_pointer_init(&pointer, &panel.device, 0, screen);
```

- `fingers` is how many the panel sees at once, at most its driver's: 5 for
  a GT911, 1 for a resistive XPT2046. `esp_lcd_touch` keeps at most
  `CONFIG_ESP_LCD_TOUCH_MAX_POINTS` of them (5 by default), so raise it for a
  panel of more.
- `numbered` says whether the driver's `track_id` stays with a finger while
  it is down (a GT911's does). Give `false` for a driver that gives none, and
  the pointer follows the fingers by where they were.
- The device's range is that of the driver's config: `0..x_max-1` and
  `0..y_max-1`.
- Leave the driver's own `swap_xy`, `mirror_x` and `mirror_y` off. The
  pointer does them, and a calibration finds them.

The header is all `static inline`, so DGX-DI does not depend on
`esp_lcd_touch`. The program that includes it adds that component and the
panel's driver.

The XPT2046 of a CYD is best taken in its ADC values: give its config
`.x_max = 4096, .y_max = 4096`, and a calibration makes them pixels. Its
driver averages `CONFIG_ESP_LCD_TOUCH_MAX_POINTS` samples a read.

### A GSL3680

The Silead GSL3680 is the touch controller of the 10.1-inch Guition
JC8012P4A1C: I2C, up to 10 fingers. ESP-IDF has no driver for it, so DGX-DI
has one. The chip keeps no firmware of its own, and the driver writes it in
at every start from data you give. Make the data once from the vendor's C
header with the range of the coordinates that firmware gives:

```sh
python tools/gsl_firmware.py vendor_firmware.h main/touch.fw --range 1664x896
```

`--array NAME` picks an array that is not called `GSLX680_FW`. The range is
not in the array: it is 64 a channel of the panel's sensor (26 by 14 on the
JC8012P4A1C), as the vendor's driver says or a touch in the corners shows.
The firmware is the vendor's, so it is not a part of DGX-DI.

Embed the file in the program (`EMBED_FILES "touch.fw"` in
`main/CMakeLists.txt`) and start the chip on the board's bus:

```c
#include "dgx_di_gsl3680.h"

extern const uint8_t touch_fw_start[] asm("_binary_touch_fw_start");
extern const uint8_t touch_fw_end[] asm("_binary_touch_fw_end");

static dgx_di_gsl3680_t gsl;

if (!dgx_di_gsl3680_init(&gsl, i2c, /* reset */ 22, /* int */ 21, /* 0: 400 kHz */ 0,
                         touch_fw_start, touch_fw_end - touch_fw_start)) {
    // it does not answer, the data is not such a firmware, or it does not start
}
dgx_di_pointer_init(&pointer, &gsl.device, DGX_DI_SWAP_XY | DGX_DI_MIRROR_Y, screen);
```

Either GPIO may be -1 when it is not wired. The chip numbers its fingers, but
its numbers do not stay with a finger, so the driver gives none and the
pointer follows the fingers by nearness.

### Your own driver

A device is a struct of the driver that begins with a `dgx_di_device_t`: a
function that reads the fingers on the panel now, how many it sees at once,
and the range of its coordinates.

```c
typedef struct {
    dgx_di_device_t device; // first: the pointer is given &my.device
    int             cs;     // the driver's own
} my_panel_t;

static int my_read(dgx_di_device_t *device, dgx_di_point_t *points, int max)
{
    my_panel_t *my = (my_panel_t *)device;
    // read the chip, then for each finger, at most max of them:
    points[0] = (dgx_di_point_t){.x = 1234, .y = 567, .size = 0, .id = DGX_DI_NO_ID};
    return 1; // how many; 0: none, or not read
}

static my_panel_t my = {.device = {.read = my_read, .fingers = 1, .x_min = 0, .x_max = 4095, .y_min = 0, .y_max = 4095}};
```

The points are in the device's own coordinates; the pointer does the rest.

### Calibrate a board once

A calibration says how a board's panel lies under its display and how far
off it takes touches. It is measured once on the board, by the developer,
and kept in the program. Run the measure instead of the program, or behind a
switch of the build, from the task that owns the display:

```c
#include "dgx_di_calibrate.h"

dgx_di_calibration_t measured;
dgx_di_calibrate_measure(&panel.device, screen, /* rounds */ 3, &measured);
```

A target blinks in each corner, at the middle of each edge and in the
middle, one after another. Put a finger down on it and hold it a moment: a
finger already down does not count, it has to come down on the target. From
the second round on, a dot shows where the panel sees the finger before it
is pressed and a ring where the press is taken, by the calibration of the
rounds before. After each round the log tells how far off the targets were
and prints the calibration so far, in words and as C:

```
The panel lies under the display of 800x1280 (its own orientation) swapped, its y the other way; its touches were taken
18.1 pixels off on average and 22 at worst without this calibration, 5.0 and 11 with it (3 rounds). ...
the calibration of 3 rounds: {.version = 1, .size = sizeof(dgx_di_calibration_t), .x_min = 0, .x_max = 1663, ...}
```

Paste the last one into the program and give it to the pointer:

```c
static const dgx_di_calibration_t board_touch = {.version = 1, .size = sizeof(dgx_di_calibration_t), /* ...as logged */};

if (!dgx_di_pointer_calibrate(&pointer, &board_touch)) {
    // not for this device or this display: measure the board again
}
```

What it finds and keeps:

- **how the panel lies**: which of its axes goes along which of the
  display's, and which way, so a board never seen works without a guess;
- **an affine** of least squares over all the targets, for a panel that is
  skewed, turned a little, stretched or shifted;
- **up to three points along each axis** for what is left, such as an edge
  that is pressed together;
- **how to steady it**: near the glass a sensitive panel shows a finger
  before it is pressed, and loses a finger held now and then. The longest of
  each makes `down_ms` and `up_ms`, and `up_ms` is 60 ms at least;
- how far off the targets were without it and with it, for the record.

Each target is taken where the median of its touches has it, over the last
15 rounds, so a touch beside the target does not move it. Three rounds are
enough; `rounds = 0` goes on without end, which is a good way to watch a
panel. The measure takes some 3 KB of the task's stack (the ESP32's main
task has 3.5 KB by default, so give it more) and 2 KB of the heap.

The calibration is a flat struct with no pointers, so it may be kept in NVS
as a blob as well. Its `version` and `size` tell one made by another version
of DGX-DI, and `dgx_di_pointer_calibrate()` refuses it, as it refuses one
made for another range of the device or another size of the display. It is
made in the display's own orientation, so it holds however the screen is
turned later, and the measure may run in any orientation.

`dgx_di_calibration_make()` makes a calibration from targets measured some
other way, and `dgx_di_calibration_text()` and `dgx_di_calibration_code()`
write one in words and as C.

### Without a calibration

Give the pointer the flags of how the panel lies, and how to steady it:

```c
dgx_di_pointer_init(&pointer, &panel.device, DGX_DI_MIRROR_X, screen); // the CYD
dgx_di_pointer_debounce(&pointer, /* down after */ 40, /* up after */ 60);
```

The flags are those of the panel against the display in the display's own
orientation, as its controller scans it before any orientation set in DGX:
see [Coordinates and orientation](#coordinates-and-orientation). The
[boards](#boards-it-was-tried-on) below have theirs. For any other board,
measure it once: the calibration finds them and prints them in words.

### Taps, swipes from an edge, a long press

A touch tells the gestures of one finger from its points on the display.
Give it the display and its size in millimetres, measured with a ruler as
the display stands (DGX does not know the size):

```c
static dgx_di_touch_t touch;

dgx_di_touch_display(&touch, screen, /* mm */ 154, 86);
...
// each step, after the read: the first finger, or a mouse, or a script
int        x, y;
const bool down = dgx_di_pointer_first(&pointer, &x, &y);
dgx_di_touch_take(&touch, x, y, down, now_ms);

if (touch.pressed) { /* went down at touch.x, touch.y */ }
if (touch.released) { /* went up there */ }
if (touch.edge & DGX_DI_EDGE_TOP) { /* swiped down from the top edge: open a panel */ }
if (touch.swipe & DGX_DI_SWIPE_LEFT) { /* swiped to the left: the next page */ }
if (touch.long_press) { /* held still for half a second */ }
```

A touch tells one gesture, once, as it comes, the way a phone does:

- **a swipe from an edge**: down within 9 mm of an edge of the display, then
  15 mm away from it;
- **a plain swipe**: 15 mm along an axis, by the way it went (along both, the
  one gone the larger share of its way);
- **a long press**: down for 500 ms and never farther than 3 mm from where it
  went down, but not by an edge, where it may still become a swipe from it.

In millimetres they feel the same on a 2.8-inch display and a 10-inch one.
The point of a press is where the panel first saw the finger: steadying takes
a moment, and a finger that swipes from an edge is no longer at the edge by
then.

### Several fingers

All the fingers of the last read are in `pointer.fingers`, each in its slot
while it is down:

```c
for (int i = 0; i < DGX_DI_FINGERS; ++i) {
    const dgx_di_finger_t *f = &pointer.fingers[i];
    if (f->pressed) { /* one more finger down, at f->x, f->y */ }
    if (f->down) { /* down, now at f->x, f->y; f->id stays the same */ }
    if (f->released) { /* went up, at f->x, f->y */ }
}
```

The first finger, the one `dgx_di_pointer_first()` gives, went down when none
was. When it goes up and others stay, none is first until all are up: a
finger left down is no new touch.

### Buttons the program draws

Rectangles are places of the display, in its pixels as it stands, that the
program draws as buttons itself. Each look over the fingers sets their flags:

```c
static dgx_di_rect_t  buttons[2] = {{.x = 40, .y = 600, .w = 200, .h = 100}, {.x = 300, .y = 600, .w = 200, .h = 100}};
static dgx_di_rects_t rects;

dgx_di_rects_init(&rects, buttons, 2);
...
dgx_di_pointer_read(&pointer, now_ms);
dgx_di_rects_take(&rects, &pointer, now_ms);
if (buttons[0].flags & DGX_DI_RECT_CLICKED) { /* let go inside: do it */ }
if ((buttons[1].flags & DGX_DI_RECT_HELD) && (buttons[1].flags & DGX_DI_RECT_INSIDE)) { /* draw it pressed */ }
if (buttons[1].flags & DGX_DI_RECT_LONG) { /* held long inside it */ }
```

A finger belongs to the rectangle it went down in until it goes up, wherever
it goes meanwhile. So a button does not blink at its border, and a finger led
out of it and let go is told from one let go inside: only that is a click. A
long press is told once, when the finger stayed inside all the time, and a
finger let go after a long press makes no click, as on a phone. A finger that
went down beside all of them holds none, even when it goes over one later.
Several fingers hold several rectangles at once, and `fingers` tells how many
hold each. `dgx_di_rects_take()` returns the rectangle a finger went down in
at this look, or -1.

They need no layer, no mask and no PSRAM, so the CYD's resistive panel of one
finger does as well.

### A game pad over a game

The pad draws a cross, buttons and sticks into a layer of DGX
(`dgx_overlay.h`) laid translucent over the display, and makes the buttons
and axes of a game pad out of the fingers on it:

```c
#include "dgx_di_pad.h"

static dgx_di_pad_t pad;

// once: the classic pad on a display of 217 x 136 mm, laid over it at 160 of 255
dgx_di_pad_init(&pad, screen, 217, 136, dgx_di_pad_classic, DGX_DI_PAD_CLASSIC);
dgx_di_pad_show(&pad, screen, 160);

// each step: the read, then the pad, then the program's own touch
dgx_di_pointer_read(&pointer, now_ms);
dgx_di_pad_take(&pad, &pointer);
if (pad.pressed & DGX_DI_PAD_A) { /* jump */ }
if (pad.buttons & DGX_DI_PAD_LEFT) { /* walk to the left */ }

// the frame, from a virtual screen: mixed with the pad on its way to the display
dgx_vscreen8_to_screen16(screen, 0, 0, frame, lut, DGX_LUT_SCALE(4));
```

The classic pad is a cross of 40 mm under the left thumb, 30 mm from the left
and the bottom edges; A, B, X and Y of 14 mm in a diamond under the right
thumb, coloured as on an Xbox pad (A green, B red, X blue, Y yellow); BACK and
START of 10 mm at the bottom in the middle, marked with arrows.

A finger that goes down on an item of the pad is the pad's until it goes up:
it is never the program's first finger and never holds a rectangle, so take
the pad before them. How a finger holds an item:

- **a button** is held while the finger stays within one and a half of its
  radius from its middle;
- **a cross** gives what is drawn under the finger: on an arm, that way; in a
  corner between two arms, both (a diagonal); nothing near its middle. A
  cross of `DGX_DI_PAD_FOUR` ways gives only the arm the finger is nearer;
- **a stick** gives two axes by how far the finger is from its middle, its
  whole travel at its rim, and past the rim the same way. A
  `DGX_DI_PAD_FLOATING` stick has its middle where the finger went down.

There is no dead zone; the game takes that off. The buttons and axes are
those of DGX-GE's `dgx_ge_input.h` one for one, the standard layout of a game
pad (SDL's GameController, the W3C Gamepad API): `pad.buttons` held now,
`pad.pressed` and `pad.released` since the last take, `pad.axes` from
-32767 to 32767 (x to the right, y down).

An item pressed is drawn lighter, and a stick's knob follows the finger. Only
what goes to the display from virtual screens is mixed with the layer, and
the layer marks the places it changed. A program that sends the whole frame
every step needs nothing more. One that sends only what changed asks for
those places as well and sends the picture under them again:

```c
int x, y, w, h;
if (dgx_overlay_take_dirty(screen, &x, &y, &w, &h)) { /* send the picture under x, y, w, h again */ }
```

`dgx_di_pad_hide()` takes the pad off the display, and `dgx_di_pad_free()`
gives its memory back. The layer and the mask take a byte for every pixel of
the display each (2 MB at 1280x800, in PSRAM), so the pad is for boards with
PSRAM and a panel of several fingers, not for the CYD.

### A pad of your own

Items are given in tenths of a millimetre from an edge or the middle of the
display, toward its middle, so one layout fits displays of any size:

```c
static const dgx_di_pad_item_t my_pad[] = {
    // a floating stick under the left thumb: its middle 30 mm from the left and the bottom, 40 mm across
    {.kind = DGX_DI_PAD_STICK, .from_x = DGX_DI_FROM_START, .from_y = DGX_DI_FROM_END, .ways = DGX_DI_PAD_FLOATING,
     .x = 300, .y = 300, .r = 200, .out = 0 /* the left stick */},
    // A, 25 mm from the right and the bottom, 18 mm across, green
    {.kind = DGX_DI_PAD_BUTTON, .from_x = DGX_DI_FROM_END, .from_y = DGX_DI_FROM_END, .x = 250, .y = 250, .r = 90,
     .out = DGX_DI_PAD_A, .color = 0x2E65},
    // START, 20 mm right of the middle, 12 mm from the bottom
    {.kind = DGX_DI_PAD_BUTTON, .from_x = DGX_DI_FROM_MIDDLE, .from_y = DGX_DI_FROM_END, .x = 200, .y = 120, .r = 50,
     .out = DGX_DI_PAD_START},
};

dgx_di_pad_init(&pad, screen, 217, 136, my_pad, sizeof my_pad / sizeof *my_pad);
```

From the right or the bottom the distance goes leftward or upward; from the
middle it goes right and down and may be less than 0. A button's `out` is its
bit, a stick's is 0 for the left stick and 1 for the right. The color is
RGB565, 0 for grey. Where two items meet, the first in the array has the
finger. The items are kept, not copied.

### Rotate the screen

Nothing to do: the pointer takes the orientation of the screen at every
read. The program turns the display with DGX, and the fingers turn with it.
The size a touch was given in millimetres turns with it too.

Rectangles are in pixels of the display as it stands, so the program gives
them again for the new orientation, as it draws its buttons again. A pad is
made for the display as it stands when it is made: free it and make it again.

## Boards it was tried on

Each was calibrated by `dgx_di_calibrate_measure()`, three rounds. Then the
gestures were tried by a script drawn on the display itself: every swipe from
an edge, four plain swipes, a long press, two fingers at once, one held while
another moves. The pad was tried the same way: the classic pad over a moving
picture; A, B, X and Y together, up, the diagonal down and left, right with
A, START, BACK, and a circle on the cross in one touch.

| Board | Display, in its own orientation | Touch | How the panel lies (found) | Off without, with a calibration | Gestures | Pad |
| --- | --- | --- | --- | --- | --- | --- |
| Guition JC8012P4A1C, 10.1" (ESP32-P4) | JD9365 800x1280, DSI | GSL3680 at 0x40 (SDA 7, SCL 8, reset 22, int 21), `dgx_di_gsl3680.h`, `--range 1664x896` | `DGX_DI_SWAP_XY \| DGX_DI_MIRROR_Y` | 18.1 px, 5.0 | 11 of 11 | 9 of 9 |
| Guition JC1060P470, 7" (ESP32-P4) | JD9165 1024x600, DSI | GT911 at 0x5d (SDA 7, SCL 8; reset and int not needed), `esp_lcd_touch_gt911` | none | 10.1 px, 3.3 | 11 of 11 | 9 of 9 |
| CYD (ESP32-2432S028R), 2.8" (ESP32) | ILI9341 240x320, SPI | XPT2046 on its own SPI (SCK 25, MOSI 32, MISO 39, CS 33), `esp_lcd_touch_xpt2046`, its ADC values 0..4095 | `DGX_DI_MIRROR_X` | 15.6 px, 1.3 | 9 of 9 (one finger) | no |

"Off" is how far the targets were taken from where they were meant, on
average, as the measure prints it. The displays measured 217 x 136 mm,
154 x 86 mm and 58 x 43 mm, lying wide. The 10-inch sensor does not report its
last 36 pixels along the bottom of the display turned to 1280x800; the band of
an edge (9 mm) covers that.

## How it fits together

```
 touch panel
     |
   device          dgx_di_device_t: read, how many fingers, the range
     |             (the GSL3680 driver, esp_lcd_touch, your own)
     v
   pointer  <----  calibration, measured once and kept
     |             the fingers of the display: steadied, followed, turned with the screen
     +------------------+-------------------+
     v                  v                   v
    pad            rectangles             touch
 claims its      the program's      the first finger:
  fingers          buttons          gestures in mm
```

Each step of the program's loop reads the pointer, then takes the pad (it
claims its fingers), then the rectangles, then the first finger for the
touch:

```c
dgx_di_pointer_read(&pointer, now_ms);
dgx_di_pad_take(&pad, &pointer);
dgx_di_rects_take(&rects, &pointer, now_ms);
const bool down = dgx_di_pointer_first(&pointer, &x, &y);
dgx_di_touch_take(&touch, x, y, down, now_ms);
```

All of it is called from the task that owns the display, once a step of its
loop: DGX's way, where one task draws and the others send it what to draw.
Nothing here waits, locks or runs a task of its own. A panel on the bus of
the display is read between the calls of DGX, when the bus is free.

The time is the program's: `esp_timer_get_time() / 1000` on the chip, the
clock of a script in a test. Whatever the frame rate, the thresholds stay in
milliseconds and only their step grows to a frame, so let `up_ms` be no less
than the longest frame of the program.

The pointer, the making of a calibration, the touch and the rectangles are
plain C with no ESP-IDF in them, so they build and are tested on a desktop
computer. The pad draws with DGX, which builds there too.

## Coordinates and orientation

A finger goes from the device to the display in this order:

1. The device's x and y are mirrored by `DGX_DI_MIRROR_X` and
   `DGX_DI_MIRROR_Y`, then swapped by `DGX_DI_SWAP_XY`: how the panel lies
   under the display in the display's own orientation, as its controller
   scans it before any orientation set in DGX.
2. The device's range is scaled to the display's pixels.
3. The calibration's affine corrects the point, then its points along each
   axis.
4. The point is turned as the screen of DGX is turned now (its `dir_x`,
   `dir_y` and `swap_xy`).

So the flags and the calibration belong to the board and do not change when
the program turns its display. A finger's `x` and `y` are always pixels of
the display as it stands now. Its `taken_x` and `taken_y` are where it was
before the calibration and the turn, which is what a calibration is made of.

## API reference

### The device

`dgx_di.h`.

| Name | What it is |
| --- | --- |
| `dgx_di_device_t` | A panel as its driver makes it: `read`, `fingers` (at once, as the panel sees them), the range `x_min..x_max`, `y_min..y_max` along its own axes. |
| `dgx_di_read_fn` | `int read(dgx_di_device_t *device, dgx_di_point_t *points, int max)`: the fingers on the panel now, at most `max`; how many. |
| `dgx_di_point_t` | A finger as the device sees it: `x`, `y`, `size` (how hard or how wide, 0: not measured), `id` (`DGX_DI_NO_ID`: the device keeps none). |
| `DGX_DI_FINGERS` | 10: the most fingers a pointer follows. |

### The pointer

`dgx_di.h`.

| Function | What it does |
| --- | --- |
| `void dgx_di_pointer_init(dgx_di_pointer_t *pointer, dgx_di_device_t *device, int flags, const dgx_screen_t *screen)` | A pointer of a device under a display, the device's axes as the flags say. |
| `bool dgx_di_pointer_calibrate(dgx_di_pointer_t *pointer, const dgx_di_calibration_t *calibration)` | The board's calibration taken: its flags, its steadying, its correction. `false`, nothing changed: not for this device and display. `NULL`: none. Kept, not copied. |
| `void dgx_di_pointer_debounce(dgx_di_pointer_t *pointer, int down_ms, int up_ms)` | Down after the device has seen a finger `down_ms` running, up after it has not seen it `up_ms`. 0, 0 (the default): as the device says. |
| `int dgx_di_pointer_read(dgx_di_pointer_t *pointer, uint32_t now_ms)` | The device read: its fingers steadied, as fingers of the display. How many are down. |
| `bool dgx_di_pointer_first(const dgx_di_pointer_t *pointer, int *x, int *y)` | The first finger: `true` when down, and where (where it went up when not). |
| `void dgx_di_pointer_claim(dgx_di_pointer_t *pointer, int finger)` | The finger of that index taken by a part of DGX-DI until it goes up (the pad does it): never the first, holds no rectangle. |
| `void dgx_di_pointer_map(const dgx_di_pointer_t *pointer, int dx, int dy, int *x, int *y, int16_t taken[2])` | A point of the device as a point of the display; `taken`, may be `NULL`: before the calibration and the turn. |

Flags: `DGX_DI_MIRROR_X` (1), `DGX_DI_MIRROR_Y` (2), `DGX_DI_SWAP_XY` (4).

A finger, `pointer.fingers[i]` (`dgx_di_finger_t`): `x`, `y` (pixels of the
display; at the read it went down, where the device first saw it); `size`;
`id` (the device's number, or the pointer's for a device that keeps none);
`down`; `pressed` and `released` at the last read; `sees` (the device saw it
at the last read, steadied or not); `claimed`; `seen_x`, `seen_y` (where the
device saw it last); `taken_x`, `taken_y`; `landed_x`, `landed_y` (where the
device began to see it); `since_ms`.

### The calibration

`dgx_di.h` and `dgx_di_calibrate.h`.

| Function | What it does |
| --- | --- |
| `bool dgx_di_calibrate_measure(dgx_di_device_t *device, dgx_screen_t *screen, int rounds, dgx_di_calibration_t *calibration)` | Targets on the display, a finger on each, round after round; then the calibration of them all. `rounds = 0`: without end, logged. Blocks. |
| `bool dgx_di_calibration_make(dgx_di_calibration_t *calibration, const dgx_di_device_t *device, int width, int height, const dgx_di_aim_t *aims, int count)` | A calibration from targets measured another way (`dgx_di_aim_t`: where meant on the display in its own orientation, where the device took it). `false`: fewer than 4, or all in a line. |
| `void dgx_di_calibration_steady(dgx_di_calibration_t *calibration, int blink_ms, int gap_ms)` | How to steady the panel from how it blinked: down after the longest blink and a little more, up after the longest gap and a little more, 60 ms at least. |
| `int dgx_di_calibration_text(const dgx_di_calibration_t *calibration, char *text, size_t size)` | The calibration in words, one paragraph of English under 400 characters. As `snprintf`. |
| `int dgx_di_calibration_code(const dgx_di_calibration_t *calibration, char *text, size_t size)` | The calibration as C, an initializer to paste (some 470 characters). As `snprintf`. |

`dgx_di_calibration_t`: `version` (`DGX_DI_CALIBRATION_VERSION`) and `size`;
the device's range and the display's size it is for; `flags`; `affine` (x' =
(a x + b y + c) / 65536, y' = (d x + e y + f) / 65536; all 0: none);
`knots_x`, `knots_y` and the points `x[]`, `y[]` (where taken against where
meant, up to `DGX_DI_KNOTS` of each axis); `down_ms`, `up_ms`; `blink_ms`,
`gap_ms` (how the panel blinked); `off_tenths`, `worst_tenths`,
`left_tenths`, `left_worst_tenths` (how far off, without and with it, in
tenths of a pixel); `rounds`.

### The touch

`dgx_di.h`.

| Function | What it does |
| --- | --- |
| `void dgx_di_touch_display(dgx_di_touch_t *touch, const dgx_screen_t *screen, int width_mm, int height_mm)` | The display the touch is on and its size in millimetres as it stands now. `NULL` or 0: no gestures but down and up. |
| `void dgx_di_touch_take(dgx_di_touch_t *touch, int x, int y, bool down, uint32_t now_ms)` | The point as it is now: pressed, released and a gesture worked out from the last time. |

`dgx_di_touch_t`: `x`, `y`; `down`, `pressed`, `released`; `long_press`;
`edge` (`DGX_DI_EDGE_TOP`, `_BOTTOM`, `_LEFT`, `_RIGHT`: swiped from that
edge since the last look); `swipe` (`DGX_DI_SWIPE_UP`, `_DOWN`, `_LEFT`,
`_RIGHT`). The thresholds: `DGX_DI_EDGE_BAND_MM` (9),
`DGX_DI_SWIPE_TRAVEL_MM` (15), `DGX_DI_LONG_MS` (500),
`DGX_DI_LONG_SLOP_MM` (3).

### The rectangles

`dgx_di.h`.

| Function | What it does |
| --- | --- |
| `void dgx_di_rects_init(dgx_di_rects_t *rects, dgx_di_rect_t *array, int count)` | The program's rectangles to go over (taken again: their fingers let go). Kept, not copied. |
| `int dgx_di_rects_take(dgx_di_rects_t *rects, const dgx_di_pointer_t *pointer, uint32_t now_ms)` | The fingers of the last read taken: the flags of each rectangle set. The rectangle a finger went down in now (several: the first of the array), -1: none. |

`dgx_di_rect_t`: `x`, `y`, `w`, `h` (given); `flags`; `fingers` (how many of
it are down). The flags: `DGX_DI_RECT_HELD` (a finger of it is down, wherever
it is), `DGX_DI_RECT_INSIDE` (and inside it now), `DGX_DI_RECT_PRESSED`,
`DGX_DI_RECT_RELEASED`, `DGX_DI_RECT_CLICKED` (released inside, no long press
before), `DGX_DI_RECT_LONG` (held long inside, told once),
`DGX_DI_RECT_FORCED` (pressed hard: no panel sets it yet).

### The pad

`dgx_di_pad.h`.

| Function | What it does |
| --- | --- |
| `bool dgx_di_pad_init(dgx_di_pad_t *pad, const dgx_screen_t *display, int width_mm, int height_mm, const dgx_di_pad_item_t *items, int count)` | A pad of these items on the display as it stands, measured in millimetres: its layer and mask made and drawn. `false`: no memory, or more than 124 items. |
| `bool dgx_di_pad_show(dgx_di_pad_t *pad, dgx_screen_t *display, uint8_t alpha)` | Laid over the display, `alpha` 0..255 how much of its colors over the picture (160: well seen, the game too). |
| `void dgx_di_pad_hide(dgx_di_pad_t *pad, dgx_screen_t *display)` | Taken off the display, its place marked to be sent again. |
| `void dgx_di_pad_free(dgx_di_pad_t *pad, dgx_screen_t *display)` | Its layer and mask given back, hidden first. |
| `void dgx_di_pad_take(dgx_di_pad_t *pad, dgx_di_pointer_t *pointer)` | The fingers of the last read taken: those gone down on an item claimed, the buttons and axes made, the items changed drawn again. |
| `int dgx_di_pad_item_at(const dgx_di_pad_t *pad, int x, int y)` | The item at a point of the display, -1: none. |

`dgx_di_pad_item_t`: `kind` (`DGX_DI_PAD_BUTTON`, `DGX_DI_PAD_CROSS`,
`DGX_DI_PAD_STICK`); `from_x`, `from_y` (`DGX_DI_FROM_START`,
`DGX_DI_FROM_MIDDLE`, `DGX_DI_FROM_END`); `ways` (`DGX_DI_PAD_FOUR` for a
cross, `DGX_DI_PAD_FLOATING` for a stick); `x`, `y`, `r` in tenths of a
millimetre; `out`; `color`. `dgx_di_pad_classic` and `DGX_DI_PAD_CLASSIC`
(7): the classic pad.

`dgx_di_pad_t`: `buttons`, `pressed`, `released` (`DGX_DI_PAD_LEFT`, `RIGHT`,
`UP`, `DOWN`, `A`, `START`, `BACK`, `B`, `X`, `Y`, `HOME`, `LB`, `RB`, `LT`,
`RT`, `L3`, `R3`: bits 0 to 16); `axes[DGX_DI_AXES]` (`DGX_DI_AXIS_LX`, `LY`,
`RX`, `RY`, from -32767 to 32767; `LT`, `RT`, from 0); `places` (each item in
pixels: `x`, `y`, `r`).

### Drivers

| Function | What it does |
| --- | --- |
| `void dgx_di_esp_lcd_touch_init(dgx_di_esp_lcd_touch_t *panel, esp_lcd_touch_handle_t handle, int fingers, bool numbered)` | `dgx_di_esp_lcd_touch.h`: a panel of `esp_lcd_touch` as a device, `&panel.device`. |
| `bool dgx_di_gsl3680_init(dgx_di_gsl3680_t *touch, i2c_master_bus_handle_t bus, int reset_gpio, int int_gpio, int scl_hz, const uint8_t *firmware, size_t size)` | `dgx_di_gsl3680.h`: the chip reset, its firmware written in and started; the device is `&touch.device`. |

`tools/gsl_firmware.py header.h out.fw --range WxH [--array NAME]`: a Silead
firmware of a vendor's C array as the data the driver takes ("GSLF", the
number of pages, the range, then the pages of 32 words). A firmware of
another shape is refused.

## Memory

On a 32-bit chip:

| Part | RAM |
| --- | --- |
| a pointer | 308 bytes, 28 of them a finger, room for 10 |
| a touch | 32 bytes |
| rectangles | 80 bytes, and 10 bytes a rectangle in the program's array |
| a calibration | 84 bytes, read where it lies (flash, as a constant) |
| a pad | 100 bytes, 16 an item, and a byte a pixel of the display for its layer and as much for its mask |
| the measure of a board | some 3 KB of the stack and 2 KB of the heap while it runs |

## Known gaps

- No on-screen keyboard yet, and no editor of a pad's layout.
- Gestures are those of one finger, the first. There is no pinch.
- `DGX_DI_RECT_FORCED` is set by no panel yet.
- A pad does not follow a display turned after it was made.
- The pad does not fade when nobody touches it.
- No smoothing of a resistive panel on DGX-DI's side: the XPT2046 driver
  averages its samples, and a sample far off moves that mean.
- The GSL3680's fingers are followed by nearness, so two fingers that cross
  each other quickly may change places.
- Only the GSL3680 has a driver of its own; other panels come through
  `esp_lcd_touch`.

## Host tests

```sh
make -C test/host
```

They build the plain-C part with the address and undefined-behaviour
sanitizers, with DGX from the managed component of the project DGX-DI sits in
or from a checkout next to it (`../esp32-component-dgx`; `DGX=<path>` takes
another). They check:

- the pointer: the corners of a turned 10-inch panel, every flag, ranges,
  scaling, the calibration, the screen turned, fingers by the panel's numbers
  and by nearness, steadying in milliseconds, the point of a press, the first
  finger and the fingers claimed;
- the making of a calibration: a made-up panel lying each of the 8 ways,
  skewed and bent, the flags found and the targets where they were meant;
- the rectangles: a finger held by the one it went down in, clicks and their
  cancelling, a long press, several fingers, borders, the fingers claimed;
- the pad, with DGX built for the host: its items placed from millimetres,
  the finger claimed, a button within its reach, every way of a cross and its
  diagonals, a cross of four ways, sticks fixed and floating, past the rim,
  an item pressed drawn again and its neighbour not, pixels that are not
  square, shown and hidden;
- the gestures: edges, swipes, a long press, one gesture a touch, in
  millimetres.

## Repository layout

```
include/dgx_di.h                the device, the pointer, the calibration, the touch, the rectangles
include/dgx_di_calibrate.h      the measure of a board
include/dgx_di_pad.h            the game pad
include/dgx_di_gsl3680.h        the GSL3680 driver
include/dgx_di_esp_lcd_touch.h  a panel of esp_lcd_touch as a device (static inline)
src/                            their sources
tools/gsl_firmware.py           a vendor's GSL firmware as data for the driver
test/host/                      the host tests
```

## License

MIT, see [LICENSE](LICENSE).
