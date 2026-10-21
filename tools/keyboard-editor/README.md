# The keyboard editor of DGX-DI

An editor of layouts of DGX-DI's keyboard (`dgx_di_keyboard.h`), in the
browser. Open `index.html`: no server or build is needed, Vue, Pinia and
Bootstrap come from a CDN.

- **The display.** Choose one of the boards DGX-DI was tried on, or give
  your own: its pixels and its size in millimetres, as it stands in the
  program. The canvas is that display in its own pixels: a grid of
  centimetres, the program's field (at the top, at the bottom or none) and
  the band of the keyboard with its keys, placed and drawn as DGX-DI places
  and draws them. The colors are those RGB565 makes of the theme's, the
  signs of erase, done, cancel and shift the same shapes.
- **The labels.** In the browser's own font at the size given, or in one
  chosen: a font of the computer (Computer…: Chrome and Edge list them, once
  allowed), a font file (File…, or dropped: TTF, OTF, TTC), or a font of DGX
  (its C made by font2c, or the JSON of DGX-GE's `dgx_ge_font.py`). A font of
  DGX is drawn pixel for pixel where DGX-DI draws it; a font of the computer
  or of a file at its size, its pixels lit or not as DGX's are. A key whose
  letters the font has not is marked red in its corner and told among the
  problems (the letters of a font of DGX are its glyphs; of a font file, its
  table of characters). The font is saved with the layout: a font of DGX by
  its name, a face of a file by its family, style and PostScript name at a
  size in millimetres (pixels on the display shown as DGX counts them:
  across, rounded), with what the build asks DGX for below it
  (`dgx_font_need(keys LAYOUT keys.json)`, DGX 0.7.2 and later).
- **The layout.** Its sizes in millimetres: the height of a key, its width
  at most (the longest row of all pages makes it narrower to fit the
  display), the gaps, the margin of the band, the rounding of the corners.
  Its theme: the band from its top color to its bottom one, the keys, the
  special keys, a key pressed, the labels; a few themes to begin with.
- **The keys.** A click chooses a key; on the right, what it types (a
  character, and with shift), or what it does (erase, done, cancel, shift,
  to a page), its label and its width in quarters of a key. Keys and rows
  are put before or after, moved and taken away; pages are added after the
  one shown, with a key back, and taken away.
- **Trying.** In Try the keys type into the program's field, as DGX-DI
  types: let go on the key, erase held again and again, shift for one
  character, the pages.
- **To begin with** (New): DGX-DI's own layouts, a code of digits and the
  Latin letters, and others: a phone's number (`* 0 #`, a plus), a number
  with a sign and a point, hexadecimal, Russian (ЙЦУКЕН with shift, digits
  and signs, Latin; the program's font must have the Cyrillic letters),
  German (QWERTZ with ä ö ü ß, digits and signs with €). A language is a page:
  a key of a page goes to it.
- **The file.** The layout is saved as JSON (Save JSON) and opened again
  (Open, or a file dropped onto the window); the browser keeps the last one
  between visits. Undo and redo: Ctrl+Z, Ctrl+Shift+Z.

The ready layouts of DGX-DI are `examples/digits.json` and
`examples/latin.json`. `src/dgx_di_keyboard_layouts.c` is made of them; to
change one, open it here, save it over itself and make that file again:

```sh
python3 tools/keyboard_blob.py --builtin src/dgx_di_keyboard_layouts.c tools/keyboard-editor/examples/[a-z]*.json
```

A layout of a program of one's own is made a blob by the same tool, a file
to embed or a C array (`tools/keyboard_blob.py layout.json out.kbd`,
`--c NAME layout.json out.c`); the tool tells the format of the JSON and of
the blob, and refuses a layout DGX-DI would not open.

## The format

```json
{
    "format": "dgx-di-keyboard",
    "version": 1,
    "name": "latin",
    "keyH": 100, "unitW": 160, "gap": 12, "margin": 15, "radius": 15,
    "theme": {"backTop": "#2b2f36", "backBottom": "#1b1e23", "key": "#4b525c", "keySpecial": "#353a42", "keyPressed": "#8a94a3", "label": "#ffffff"},
    "pages": [
        {"name": "letters", "rows": [
            [{"char": "q", "upper": "Q"}, {"char": "w", "upper": "W"}],
            [{"action": "shift", "width": 6}, {"page": 1, "label": "123"}, {"char": " ", "width": 20}, {"action": "done"}]
        ]}
    ]
}
```

The sizes are tenths of a millimetre. A key types a character (`char`;
`upper` with shift), does an action (`erase`, `done`, `cancel`, `shift`) or
goes to a page (`page`, from 0); `label` is drawn on it instead, `width` is
in quarters of a key (4 one key, the default). At most 4 pages, 5 rows a
page, 48 keys a page.

## Tests

- `test.js`, with Node: the editor's geometry against DGX-DI's own on the
  ready layouts and several displays, the ready layouts written as the
  editor writes them, and the templates (nothing wrong on the displays of the
  boards, and `keyboard_blob.py` makes blobs of them and gives the same
  letters); fonts of DGX read of their C and drawn as DGX draws them: the
  bounds and pixels of strings and the labels of the Latin keyboard, key by
  key, against `test/host/font_dump`; the names and letters of a TTF.
  `make -C test/host` runs it (with the C that makes what it compares).
- `test.html`, in a browser: the editor's actions on the layout it begins
  with, the workbench drawn meanwhile, the menu of templates, the fonts of
  the labels (a font of DGX, its keys of letters it has not marked, the
  browser's own again, a face of the computer). The line on top says how it
  went.
