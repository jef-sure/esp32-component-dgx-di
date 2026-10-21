#!/usr/bin/env python3
"""
A layout of the keyboard of DGX-DI (dgx_di_keyboard.h) as the data dgx_di_keyboard_open() takes: of the JSON that
tools/keyboard-editor writes and reads, a blob.

    keyboard_blob.py layout.json out.kbd                  the blob, a file to embed (EMBED_FILES)
    keyboard_blob.py --c NAME layout.json out.c           the blob as a C array: const uint8_t NAME[], const size_t NAME_size
    keyboard_blob.py --builtin out.c layout.json...       DGX-DI's own layouts: dgx_di_keyboard_<name> and _size

The JSON (version 1):

    {"format": "dgx-di-keyboard", "version": 1, "name": "latin",
     "keyH": 100, "unitW": 160, "gap": 12, "margin": 15, "radius": 15,       tenths of a millimetre
     "theme": {"backTop": "#2b2f36", "backBottom": "#1b1e23", "key": "#4b525c", "keySpecial": "#353a42",
               "keyPressed": "#8a94a3", "label": "#ffffff"},
     "pages": [{"name": "letters", "rows": [[key, key, ...], ...]}, ...]}

a key {"char": "q", "upper": "Q"} types a character (upper: with shift), {"action": "erase" | "done" | "cancel" |
"shift"} edits, {"page": 1} goes to that page; each may have "label" (drawn on it instead) and "width" (in quarters of
a key, 4 one key). keyH is the height of a key, unitW its width at most, gap between keys, margin between them and the
edges of the band, radius of the corners; the band from backTop to backBottom.

The blob (little-endian):

    "DGXK", u8 version (1), u8 pages (1..4), u16 keys (of all pages),
    u16 key_h, unit_w, gap, margin, radius (tenths of a millimetre),
    u16 back_top, back_bottom, key, key_special, key_pressed, label (RGB565, the low bits dropped as dgx_rgb_to_16),
    u16 name (its place in the strings; 0xffff: none)                                                     32 bytes
    each page: u8 rows (1..5), u8 keys of each of 5 rows (0 past the last)                                6 bytes
    each key, page after page, row after row: u32 code (a code point; 0x110000 shift; 0x110100 + n: to page n),
    u32 upper (with shift; 0: code), u16 label (its place in the strings; 0xffff: none), u8 width (quarters; 0: 4),
    u8 0                                                                                                 12 bytes
    the strings: UTF-8, each ended by a 0

A page has at most 48 keys. A layout of another shape is refused, with what is wrong.
"""
import argparse
import json
import os
import re
import struct
import sys

FORMAT, VERSION = "dgx-di-keyboard", 1
PAGES, ROWS, KEYS = 4, 5, 48
SHIFT, PAGE = 0x110000, 0x110100
ACTIONS = {"erase": 0x08, "done": 0x0A, "cancel": 0x1B, "shift": SHIFT}
NONE = 0xFFFF


def rgb565(color, what):
    if not isinstance(color, str) or not re.fullmatch(r"#[0-9a-fA-F]{6}", color):
        raise ValueError(f"{what}: a color is #rrggbb, not {color!r}")
    v = int(color[1:], 16)
    r, g, b = v >> 16 & 255, v >> 8 & 255, v & 255
    return (r & 0xF8) << 8 | (g & 0xFC) << 3 | (b & 0xF8) >> 3


def one(text, what):
    if not isinstance(text, str) or len(text) != 1:
        raise ValueError(f"{what}: one character, not {text!r}")
    return ord(text)


def code(key, pages, where):
    kinds = [k for k in ("char", "action", "page") if k in key]
    if len(kinds) != 1:
        raise ValueError(f"{where}: a key types a char, does an action or goes to a page: one of them")
    if "char" in key:
        return one(key["char"], where)
    if "action" in key:
        if key["action"] not in ACTIONS:
            raise ValueError(f"{where}: no action {key['action']!r} (erase, done, cancel, shift)")
        return ACTIONS[key["action"]]
    if not isinstance(key["page"], int) or not 0 <= key["page"] < pages:
        raise ValueError(f"{where}: no page {key['page']!r}")
    return PAGE + key["page"]


def tenths(layout, name, least):
    v = layout.get(name)
    if not isinstance(v, int) or not least <= v <= 32767:
        raise ValueError(f"{name}: a whole number of tenths of a millimetre from {least}, not {v!r}")
    return v


class Strings:
    def __init__(self):
        self.data, self.at = bytearray(), {}

    def add(self, text):
        if text is None:
            return NONE
        if text not in self.at:
            if len(self.data) > 0xFFFE:
                raise ValueError("the labels take more than 64 KB")
            self.at[text] = len(self.data)
            self.data += text.encode("utf-8") + b"\0"
        return self.at[text]


def blob(layout):
    if not isinstance(layout, dict) or layout.get("format") != FORMAT:
        raise ValueError(f"not a layout of the keyboard of DGX-DI (format {FORMAT!r})")
    if not isinstance(layout.get("version"), int) or layout["version"] > VERSION:
        raise ValueError(f"a layout of version {layout.get('version')!r}: this tool reads up to {VERSION}")
    pages = layout.get("pages")
    if not isinstance(pages, list) or not 1 <= len(pages) <= PAGES:
        raise ValueError(f"a layout has 1 to {PAGES} pages")
    theme = layout.get("theme") or {}
    if not isinstance(theme, dict):
        raise ValueError(f"theme: an object, not {theme!r}")
    strings, head, keys = Strings(), bytearray(), bytearray()
    count = 0
    for p, page in enumerate(pages):
        if not isinstance(page, dict):
            raise ValueError(f"page {p + 1}: an object, not {page!r}")
        rows = page.get("rows")
        if not isinstance(rows, list) or not 1 <= len(rows) <= ROWS:
            raise ValueError(f"page {p + 1}: 1 to {ROWS} rows")
        for r, row in enumerate(rows):
            if not isinstance(row, list) or not row:
                raise ValueError(f"page {p + 1}, row {r + 1}: no keys")
        if sum(len(row) for row in rows) > KEYS:
            raise ValueError(f"page {p + 1}: at most {KEYS} keys")
        head += struct.pack("<B5B", len(rows), *([len(row) for row in rows] + [0] * (ROWS - len(rows))))
        for r, row in enumerate(rows):
            for i, key in enumerate(row):
                where = f"page {p + 1}, row {r + 1}, key {i + 1}"
                if not isinstance(key, dict):
                    raise ValueError(f"{where}: an object, not {key!r}")
                c = code(key, len(pages), where)
                upper = one(key["upper"], where + ", with shift") if key.get("upper") else 0
                width = key.get("width", 4)
                if not isinstance(width, int) or not 1 <= width <= 255:
                    raise ValueError(f"{where}: a width of 1 to 255 quarters, not {width!r}")
                label = key.get("label") or None
                keys += struct.pack("<IIHBB", c, upper, strings.add(label), 0 if width == 4 else width, 0)
                count += 1
    name = strings.add(layout.get("name") or None)
    colors = [rgb565(theme.get(k), f"theme, {k}") for k in ("backTop", "backBottom", "key", "keySpecial", "keyPressed", "label")]
    header = b"DGXK" + struct.pack("<BBH5H6HH", VERSION, len(pages), count, tenths(layout, "keyH", 1), tenths(layout, "unitW", 1),
                                   tenths(layout, "gap", 0), tenths(layout, "margin", 0), tenths(layout, "radius", 0), *colors, name)
    return bytes(header + head + keys + strings.data)


def ident(name):
    s = re.sub(r"[^a-z0-9]+", "_", (name or "keyboard").lower()).strip("_") or "keyboard"
    return "_" + s if s[0].isdigit() else s


def array(name, data, source):
    lines = [f"/* {source}: {len(data)} bytes */", f"const uint8_t {name}[] = {{"]
    for i in range(0, len(data), 16):
        lines.append("    " + ", ".join(f"0x{b:02x}" for b in data[i:i + 16]) + ",")
    lines += ["};", f"const size_t {name}_size = sizeof {name};", ""]
    return "\n".join(lines)


def read(path):
    with open(path, encoding="utf-8") as f:
        return json.load(f)


def main():
    ap = argparse.ArgumentParser(description="A layout of the keyboard of DGX-DI (JSON of tools/keyboard-editor) as a blob.")
    ap.add_argument("--c", metavar="NAME", help="write a C array NAME[] and NAME_size, not a binary file")
    ap.add_argument("--builtin", metavar="OUT", help="write DGX-DI's own layouts of the JSON files given into OUT")
    ap.add_argument("files", nargs="+", help="layout.json out (or, with --builtin, the layouts)")
    a = ap.parse_args()
    try:
        if a.builtin:
            out = ["/*", " * The ready layouts of the keyboard of DGX-DI (dgx_di_keyboard.h): blobs made by tools/keyboard_blob.py of the JSON",
                   " * files in tools/keyboard-editor/examples. Change those (in the editor) and make this again:",
                   " *", " *     python3 tools/keyboard_blob.py --builtin src/dgx_di_keyboard_layouts.c tools/keyboard-editor/examples/[a-z]*.json",
                   " */", '#include "dgx_di_keyboard.h"', ""]
            for path in sorted(a.files, key=os.path.basename):
                layout = read(path)
                out.append(array("dgx_di_keyboard_" + ident(layout.get("name")), blob(layout), os.path.basename(path)))
            with open(a.builtin, "w", encoding="utf-8") as f:
                f.write("\n".join(out))
            return
        if len(a.files) != 2:
            ap.error("a layout and where to write it")
        data = blob(read(a.files[0]))
        if a.c:
            text = "\n".join([f"/* The keyboard layout of {os.path.basename(a.files[0])}, made by tools/keyboard_blob.py of DGX-DI: dgx_di_keyboard_open(..., {a.c}, {a.c}_size, ...) */",
                              "#include <stddef.h>", "#include <stdint.h>", "", array(a.c, data, os.path.basename(a.files[0]))])
            with open(a.files[1], "w", encoding="utf-8") as f:
                f.write(text)
        else:
            with open(a.files[1], "wb") as f:
                f.write(data)
    except (OSError, ValueError, json.JSONDecodeError) as e:
        sys.exit(f"keyboard_blob.py: {e}")


if __name__ == "__main__":
    main()
