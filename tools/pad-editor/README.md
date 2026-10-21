# The pad editor of DGX-DI

An editor of layouts of DGX-DI's pad (`dgx_di_pad.h`) and of how their items
look, in the browser. Open `index.html`: no server or build is needed, Vue,
Pinia and Bootstrap come from a CDN.

- **To begin with** (New): a menu of templates, each drawn on the display
  chosen: DGX-DI's own layouts (classic, flat, twin) and the pads of the
  usual games, in the default look: retro (a cross, B and A, as NES and Game
  Boy), super (the classic one and the shoulders LB and RB, as SNES), arcade
  (a cross and six buttons in two rows, for fighting), shooter (a stick to
  go, a stick to aim, LT and RT), platformer (left and right, a jump and a
  shot), racing (left and right to steer, LT to brake and RT to go), and
  mini, for a display of 2.8" as the CYD's.
- **The display.** One of the boards DGX-DI was tried on, or your own: its
  pixels and its size in millimetres, as it stands in the program. The
  canvas is that display in its own pixels, with a grid of centimetres over
  it (the grid button at the top turns it off). Under the pad lies a game
  made up (sky, sun, hills, ground), a plain dark or light color, or an image
  of your own. Over it the pad's layer is mixed at the alpha you set, as DGX
  mixes a layer (in 32 steps, RGB565), so the canvas shows what the display
  will.
- **The items.** A click chooses one: its circle, the one a finger takes it
  by, shows dashed; the one under the mouse is outlined. Drag it by its
  middle and resize it by its rim, a millimetre a step (with Alt, a tenth);
  the arrow keys nudge it a millimetre (Shift 5, Alt a tenth); its pictures
  grow with it. On the right are its kind, its button (a chip for each bit
  of DGX-GE) or stick (left, right; floating), a cross of four ways, the edge
  or the middle of the display its place is measured from (a grid of nine)
  and its distances in millimetres, its radius (and how wide it is across),
  its color (a swatch or your own). Items are added in the middle of the
  display, copied beside (Ctrl+D), moved up and down the list (the first has
  the finger where two overlap) and taken away (Delete). A line under the
  display says what the mouse does now.
- **Looks.** Pictures for every item at once (Flat, Glass, Outline, Neon),
  each shown as it draws a cross and a button; Default takes them away, and
  the items look as DGX-DI draws them by itself. An item takes a look of its
  own the same way, shown on that item.
- **Pictures.** Draw its pictures opens an item's pictures in place of the
  display, large: a button's `normal` and `pressed`; a cross's `normal`,
  `up`, `down`, `left`, `right` (an arm held, drawn over the normal) and
  `upLeft`, `upRight`, `downLeft`, `downRight` (a corner held: both its ways,
  over the arms); a stick's `well`, `knob` and `knobHeld` (the knob drawn
  where the finger has it, half its way); each a tab with the item drawn in
  that state. Beside the picture the item is drawn in the pixels of the
  display, as DGX-DI will draw it. The pictures are drawn of shapes in tenths
  of a millimetre from the item's middle: a circle (from its middle out), a
  ring, a rectangle (corner to corner) with round corners, a convex polygon
  (a click a point, a double click or Enter ends it), a line drawn by hand.
  A shape's color is its own, or the item's (`item`), lighter (`light`) or
  darker (`dark`). Shapes are chosen and moved, given their sizes in
  millimetres, ordered, copied, taken away (Delete). The pictures go to
  every item of the kind, as large as each, and a cross's arms held are made
  of the one up. Done (or Escape) goes back to the display.
- **Trying.** In Try the mouse is a finger, as DGX-DI takes one: a button held
  within one and a half of its radius, a cross as it is drawn, a stick by
  how far; on the right the buttons held light up and the sticks show their
  knobs and axes.
- **The file.** The layout is saved as JSON (Save JSON) and opened again
  (Open, or a file dropped onto the window); the browser keeps the last one
  between visits. Undo and Redo at the top (Ctrl+Z, Ctrl+Shift+Z or Ctrl+Y)
  take back and bring back any change of the layout, an item taken away
  among them (an Undo is offered in the line below too), on any layout of
  the keyboard (Ctrl+Я is Ctrl+Z); the steps live
  while the tab does, through a reload. Revert takes every change back at
  once: to the layout as it was opened, begun of a template or last saved as
  JSON, a step undone like any other. The problems of a
  layout are listed on the right, and beside the item they belong to: errors
  (the program refuses it) and warnings (an item past the edge, under 8 mm
  across, two that overlap).
- **For the program:** the blob `dgx_di_pad_open()` takes, made in the
  browser: a `.pad` file to embed (`EMBED_FILES`), a `.c` file with the
  array, or the array in the clipboard. It is the same, byte for byte, as
  `tools/pad_blob.py` makes of the JSON (the tests compare them), and a
  layout the program would not open is refused with what is wrong.
- **Help:** `?` at the top right (or H).

`tools/pad_blob.py` makes of the JSON the blob `dgx_di_pad_open()` takes, a
file to embed or a C array; it tells the format of the JSON and of the blob.
The ready layouts of DGX-DI are `examples/classic.json` (the classic pad,
drawn by default), `examples/flat.json` (the same of plain shapes) and
`examples/twin.json` (a floating stick, A B X Y, LB and RB, BACK and START, of
glass). `src/dgx_di_pad_layouts.c` is made of them; to change one, open it
here, save it over itself and make that file again:

```sh
python3 tools/pad_blob.py --builtin src/dgx_di_pad_layouts.c tools/pad-editor/examples/[a-z]*.json
```

## Tests

- `test.js`, with Node: the places of the items against DGX-DI's own on the
  ready layouts and several displays, colors mixed as DGX mixes them, the
  fingers as DGX-DI takes them, the ready layouts written as the editor
  writes them, the templates (nothing wrong on the displays they are for,
  and `pad_blob.py` makes blobs of them), the blob as the editor makes it
  (`make -C test/host` compares it with `pad_blob.py`'s, byte for byte, on
  every template and every look). `make -C test/host` runs it, with the C
  that makes what it compares.
- `test.html`, in a browser: the editor's actions (items moved, nudged,
  copied, ordered; looks; the anchor and the chips of a button; the blob),
  and the mouse on the display and in a picture (an item dragged; a circle,
  a polygon, a line drawn; a shape moved), the menu of templates with its
  thumbnails, the readout of Try, the help. The line on top says how it
  went.
