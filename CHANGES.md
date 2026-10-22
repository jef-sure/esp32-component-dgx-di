# Changes

## 0.1.2 - 2026-10-22

On DGX 0.8: a keyboard's font made by DGX at the build, of the layout's face
and size with the letters of its labels, and the keyboard editor drawing the
labels in that font. Checked by the host tests against DGX 0.8.0 and by
ESP-IDF 5.5.5, 6.0.3 and 6.1 builds for the ESP32 and the ESP32-P4 with DGX
0.8.0 of the registry and a keyboard font made at the build.

- **DGX 0.8** (`jef-sure/dgx` ^0.8.0, 0.7 no more). DGX-DI used none of
  what 0.8 took away (Arial and Verdana, for their licenses): only its host
  test of the editor's labels drew in them, and it draws in their free
  successors, `LiberationSansRegular12()` and `DejaVuSansBook32()`, the
  capital of the first still of an odd height. The host tests take DGX's
  sources and stubs of ESP-IDF from DGX's own `host/dgx_host.mk`, so the
  list follows DGX.
- **The keyboard editor draws the labels in the font chosen:** the
  browser's own, a font of the computer (Chrome and Edge list them), a font
  file (TTF, OTF), or a font of DGX (the C of font2c, the JSON of DGX-GE's
  `dgx_ge_font.py`), drawn pixel for pixel where DGX-DI draws it (compared
  with DGX's own drawing, glyph and label). A key whose letters the font has
  not is marked and told among the problems. The font is saved with the
  layout (`"font"`: a face by its PostScript name at a size in millimetres,
  or a font of DGX by its name). The blob is the same as before.
- **Undo in the keyboard editor** on any layout of the computer's keyboard
  (Ctrl+Я is Ctrl+Z on ЙЦУКЕН, which typed nothing) and from a list; its
  steps live while the tab does, through a reload.
- **`dgx_di_keyboard_font(<name> <layout.json> [FILE <font.c>])`** for a
  program's CMakeLists (DGX-DI's `project_include.cmake`): the font of a
  layout's labels asked of DGX's font needs, its face and size as the
  editor saved them, the letters of its labels in a file of the build,
  written only when they change; DGX need not know the layout. The program
  gives the keyboard `dgx_font_<name>()`.
- `tools/keyboard_blob.py --letters OUT layout.json` writes the letters of
  the labels past ASCII, for a font made by hand with `font2c -f OUT`.
- On the 7-inch board the keyboard of the Russian template typed all its
  tasks in a font DGX 0.7.2 made of the layout at the build; on DGX 0.8 it
  is built, not yet tried on a board.

## 0.1.1 - 2026-10-21

On DGX 0.7, which has its sources grouped by topic and mixes the overlay
layer into every transfer of a screen, the turned ones too. DGX-DI uses none
of what 0.7 took away (the oriented functions of a virtual screen, the
pixels of an area one by one, dgx_bw_page_screen.h).

- **DGX 0.7** (`jef-sure/dgx` ^0.7.0, 0.6 no more). The host tests build
  DGX's sources where the checkout given has them, 0.7 or 0.6, and pass
  against DGX 0.7.3 and 0.6.6; the board programs build with ESP-IDF 5.5.5,
  6.0.3 and 6.1 for the CYD and the 7- and 10-inch boards (with DGX 0.7.0).
- **Quick taps of two keys both typed.** A finger lost a moment is held
  down for `up_ms`, and it took any point meanwhile: a tap of another key
  less than `up_ms` after the last went up moved the last finger there, with
  no press (a GSL3680 calibrated at 110 ms lost the second digit of "02"
  typed quickly; a GT911 giving the new finger the number 0 again, the
  same). Now a finger lost takes only a point within an eighth of the
  display's smaller side of its last place; one farther is a new finger,
  pressed where it lands, with the device's number or with none.
- The GSL3680 read: a count of more than ten fingers (a read as the chip
  resets, `0xff` throughout) is not taken for points.
- `dgx_di_pad_hide()` (and `dgx_di_pad_free()`) leaves another layer laid
  over the display.
- `tools/gsl_firmware.py`, `tools/pad_blob.py` and `tools/keyboard_blob.py`
  refuse an array not closed, a word past 32 bits, an item, shape, page, row
  or key that is not an object, with what is wrong instead of a traceback; a
  color missing from the keyboard's theme is named.
- The package of the registry leaves the host tests out; they are in the
  repository.
- **README:** installed from the registry, DGX-DI and DGX both go into
  `managed_components/`, and that is all. Only a checkout of DGX of one's
  own, in place of the registry's, must sit in a directory named
  `jef-sure__dgx`, in `components/` or where an `override_path` points: ESP-IDF
  names a local component after its directory.

## 0.1.0 - 2026-10-21

- **The pad editor made friendlier.** New shows each template drawn on the
  display chosen; the looks (Default, Flat, Glass, Outline, Neon) are shown
  as they draw a cross and a button, for every item and for one; a button's
  bit is a chip, the edge or the middle an item's place is measured from a
  grid of nine, its color a swatch; items are copied, nudged by the arrow
  keys, ordered, taken away by Delete, their names in words (Button A, Cross,
  Left stick). An item's pictures open in place of the display, large, each
  state a tab with the item drawn in it, and beside them the item in the
  pixels of the display, as DGX-DI will draw it; a line under the display
  says what the mouse does now. Try lights the buttons held and shows the
  sticks' knobs and axes on the right. For the program gives the blob
  `dgx_di_pad_open()` takes straight from the browser (a `.pad` file, a C
  file, or the C array in the clipboard), the same bytes as
  `tools/pad_blob.py` makes, and refuses a layout the program would refuse,
  with what is wrong. A help (`?`) tells how the editor is used; a grid of
  centimetres lies over the display. Undo and Redo are named at the top and
  offered under the display after an item is taken away, work on any layout
  of the keyboard (Ctrl+Я is Ctrl+Z) and from any control but a field of
  text, and their steps live through a reload of the tab; Revert takes every
  change back at once, to the layout as it was opened, begun of a template
  or last saved.

## 0.0.2 - 2026-10-20

On DGX 0.6.6, as 0.0.1.

- **A keyboard on the display** (`dgx_di_keyboard.h`). Opaque keys in a
  band of the display, sized in millimetres, at the bottom in the middle or
  at the top when the band would cover the program's field; drawn straight
  on the display in batches of DGX, a key pressed drawn so. What the fingers
  type comes out as code points, as from a keyboard of wires: characters,
  U+0008 erase, U+000A done, U+001B cancel. A key types when let go inside
  it, however long held, nothing when the finger is led out; fingers type
  over one another; erase held types again and again; shift for one
  character; pages. A finger on the band is the keyboard's, never the
  program's first nor one of its rectangles. No layer, no PSRAM: 764 bytes
  and the layout where it lies.
- **Its layouts are data:** JSON, made in the keyboard editor
  (`tools/keyboard-editor`, in the browser: a display chosen by its pixels
  and millimetres, the layout drawn on it as DGX-DI draws it, its keys, rows,
  pages, sizes and colors edited, typing tried), and a blob of it made by
  `tools/keyboard_blob.py`, a file to embed or a C array, which the keyboard
  opens and checks. The ready ones: `dgx_di_keyboard_digits` (a code) and
  `dgx_di_keyboard_latin` (letters with shift, digits and signs).
- **A pad of a layout made in an editor** (`dgx_di_pad_open`). Its items
  and how each looks are JSON, made in the pad editor (`tools/pad-editor`,
  in the browser: a display chosen by its pixels and millimetres, a game
  made up under the pad, the layer mixed over it as DGX mixes it; items
  dragged and resized, their kinds, buttons, places and colors; themes;
  pictures drawn of shapes; fingers tried), and a blob of it made by
  `tools/pad_blob.py`. Each item may have pictures of its own — a button's
  normal and pressed, a cross's normal and an arm held each way, a stick's
  well, knob and knob held — of circles, rings, rectangles with round
  corners, convex polygons and lines drawn by hand, in tenths of a
  millimetre, drawn by DGX in the pixels of the display it is opened on; a
  shape's color its own, or the item's, lighter or darker. The layer's
  colors are those the layout uses. Ready layouts as blobs: classic, flat,
  twin. `dgx_di_pad_init` and `dgx_di_pad_classic` stay as they were.
- **The editors begin of templates** (New): the pad editor of the pads of
  the usual games (retro, super, arcade, shooter, platformer, racing, mini
  for 2.8"), the keyboard editor of a phone's number, a number, hexadecimal,
  Russian and German, besides DGX-DI's own layouts.
- **A cross drawn as the flat skin of Delta draws one:** a plus with a
  light rim, as the buttons have, round corners, a chevron on each arm and a
  ring round its middle, where a finger holds no way; an arm held lighter,
  its chevron white. The editor's themes have the chevrons too.
- **A cross shows its diagonals:** it lies on a round plate, darker, all of
  which a finger takes, with a bracket in each corner between two arms,
  white while that diagonal is held (none on a cross of four ways). The
  corners of a cross drawn as a plus were empty, and a finger there, which
  holds both ways, did not look like it would (the 7-inch board's first try
  told so). In pictures of their own, a cross has a corner held of each
  diagonal besides its arms held.
- **A button of a way has its chevron:** LEFT, RIGHT, UP and DOWN drawn by
  default, and in the editor's themes, with a chevron as a cross's, as START,
  BACK and HOME have their signs.
- **A finger is up after 30 ms without it at least, not 60** (a
  calibration's steadying): two quick taps of a key on the 7-inch board
  lifted for 40 ms between them and were one. A panel that loses a finger
  held for longer has that from its calibration anyway.
- **A device's numbers of fingers are not taken blindly.** A read with a
  number twice is torn and not taken; a point of one finger's number lying
  on another finger, far from its own, is that finger's echo. The GT911 of
  the 7-inch JC1060P470 tears a read so when one of two fingers goes up: the
  finger going up seemed to leave from under the other, and a key held while
  another was typed was not typed.

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
