# Changes

## 0.0.1 - 2026-10-19

The first version. It stands on DGX 0.6.6.

- **A device and a pointer** (`dgx_di.h`). A device is a touch panel's
  driver: the fingers on it, with the numbers the panel keeps for them, and
  the range of its coordinates. A pointer makes them fingers of the display:
  mirrored and swapped as the panel lies under the display, scaled to its
  pixels, corrected by the board's calibration, and turned as the screen of
  DGX is turned now, so the fingers turn with the display. Up to 10 fingers,
  followed by the panel's numbers or, for a panel that keeps none, by
  nearness; down and up steadied for each finger in milliseconds; the point
  of a press where the panel first saw the finger; the first finger, for a
  program of one pointer.
- **A calibration measured once on the board** (`dgx_di_calibrate.h`).
  Targets blink on the display in the corners, at the middles of the edges
  and in the middle, a finger goes down on each, round after round. It finds
  how the panel lies by itself, corrects a skew by an affine of least squares
  and what bends by points along each axis, takes each target at the median
  of its touches, and tells from how the panel blinks how to steady it (up
  after 60 ms at least). After each round it logs the result in words and as
  C to paste into the program; a flat struct, it may be kept in NVS as well.
- **The gestures of a touch, in millimetres.** Pressed and released; a swipe
  from an edge (down within 9 mm of it, then 15 mm away), a plain swipe
  (15 mm), a long press (500 ms within 3 mm, not at an edge). One gesture a
  touch, told once, as on a phone. The touch takes points from anything: the
  first finger, a mouse, a script.
- **Rectangles for buttons the program draws.** A finger belongs to the one
  it went down in until it goes up: held, inside, pressed, released, clicked
  (let go inside, no long press before), a long press. Several fingers at
  once. No layer, no PSRAM: a resistive panel of one finger does as well.
- **A game pad on the display** (`dgx_di_pad.h`). A cross, buttons and
  sticks given in millimetres from an edge or the middle of the display,
  drawn translucent over a game in a layer of DGX; the buttons and axes of
  DGX-GE out of it, one for one. A finger that goes down on it is the pad's
  until it goes up, never the program's first finger nor a rectangle's. A
  button is held within one and a half of its radius, a cross gives what is
  drawn under the finger (an arm its way, a corner between two arms both), a
  stick its whole travel at its rim, its middle fixed or where the finger went
  down. An item pressed is drawn lighter, and its place is marked for the
  owner of the display to send again. The classic layout is ready:
  `dgx_di_pad_classic`.
- **A driver of the Silead GSL3680** (`dgx_di_gsl3680.h`) for all its 10
  fingers, its firmware and the range of its coordinates given as data, and
  `tools/gsl_firmware.py`, which makes that data from a vendor's C array.
- **Any panel of ESP-IDF's `esp_lcd_touch`** as a device
  (`dgx_di_esp_lcd_touch.h`, all `static inline`: DGX-DI does not depend on
  `esp_lcd_touch`).
- **Tests on the host**, with the sanitizers: the pointer, the making of a
  calibration, the gestures, the rectangles, and the pad with DGX built for
  the host.

Tried on the 10.1-inch Guition JC8012P4A1C (GSL3680), the 7-inch Guition
JC1060P470 (GT911) and the CYD (XPT2046): each calibrated, the gestures 11 of
11 on the two Guition boards and 9 of 9 on the CYD, the pad 9 of 9 on both
Guition boards. Built with ESP-IDF 5.5.5, 6.0.3 and 6.1.
