#!/usr/bin/env python3
"""
A layout of the pad of DGX-DI (dgx_di_pad.h) as the data dgx_di_pad_open() takes: of the JSON that tools/pad-editor
writes and reads, a blob.

    pad_blob.py layout.json out.pad                    the blob, a file to embed (EMBED_FILES)
    pad_blob.py --c NAME layout.json out.c             the blob as a C array: const uint8_t NAME[], const size_t NAME_size
    pad_blob.py --builtin out.c layout.json...         DGX-DI's own layouts: dgx_di_pad_layout_<name> and _size

The JSON (version 1), sizes in tenths of a millimetre:

    {"format": "dgx-di-pad", "version": 1, "name": "classic",
     "items": [{"kind": "cross", "fromX": "start", "fromY": "end", "x": 300, "y": 300, "r": 200},
               {"kind": "button", "fromX": "end", "fromY": "end", "x": 300, "y": 220, "r": 70, "out": "A",
                "color": "#29cf29", "pictures": {"normal": [shape, ...], "pressed": [shape, ...]}}, ...]}

an item: its kind (button, cross, stick); its place from an edge or the middle of the display toward its middle (from
the end, leftward or upward; from the middle, right and down), and its radius: the circle a finger takes it by; a
button's out, the name of its bit (LEFT RIGHT UP DOWN A START BACK B X Y HOME LB RB LT RT L3 R3), a stick's left or
right; four (a cross of four ways), floating (a stick's middle where the finger went down); its color (#rrggbb; none:
grey). With pictures, it is drawn by them, else as by default: a button's normal and pressed; a cross's normal, up,
down, left, right (an arm held, over the normal) and upLeft, upRight, downLeft, downRight (a corner held: both its
ways, over the arms); a stick's well, knob and knobHeld (the knob where the finger has it). A picture is a list of
shapes drawn in order, from the item's middle (y down):

    {"shape": "circle", "x": 0, "y": 0, "r": 70, "color": "item"}
    {"shape": "ring", "x": 0, "y": 0, "r": 70, "width": 6, "color": "#e1e1e1"}
    {"shape": "rect", "x": 0, "y": 0, "w": 80, "h": 40, "r": 10, "color": "light"}     its middle at x, y
    {"shape": "polygon", "points": [[-20, -20], [-20, 20], [20, 0]], "color": "#ffffff"}  convex
    {"shape": "line", "points": [[-30, 0], [0, -20], [30, 0]], "width": 8, "color": "dark"}

a color of a shape: #rrggbb, or item (the item's), light (half way to white), dark (half way to black).

The blob (little-endian):

    "DGXP", u8 version (1), u8 items (1..124), u16 shapes, u16 points, u16 name (in the strings; 0xffff: none), u16 0,
    u16 0                                                                                                 16 bytes
    each item: u8 kind (0 button, 1 cross, 2 stick), u8 from_x, u8 from_y (0 start, 1 middle, 2 end), u8 ways (1 four,
    2 floating), i16 x, i16 y, i16 r, u16 color (RGB565, the low bits dropped as dgx_rgb_to_16; 0 grey), u32 out (a
    bit; a stick 0 left, 1 right), 9 x (u16 first shape, u16 shapes) its pictures in the order above, u8 pictured,
    u8 0, u16 0                                                                                           56 bytes
    each shape: u8 shape (0 circle, 1 ring, 2 rect, 3 polygon, 4 line), u8 paint (0 its color, 1 item, 2 light,
    3 dark), u16 color, i16 x, i16 y, i16 w (a ring's width), i16 h, i16 r (a rect's corners, a line's width), u16 first
    point, u16 points, u16 0                                                                              20 bytes
    each point: i16 x, i16 y                                                                               4 bytes
    the strings: UTF-8, each ended by a 0

A layout of another shape is refused, with what is wrong.
"""
import argparse
import json
import os
import re
import struct
import sys

FORMAT, VERSION = "dgx-di-pad", 1
ITEMS = 124
KINDS = {"button": 0, "cross": 1, "stick": 2}
FROM = {"start": 0, "middle": 1, "end": 2}
BITS = ["LEFT", "RIGHT", "UP", "DOWN", "A", "START", "BACK", "B", "X", "Y", "HOME", "LB", "RB", "LT", "RT", "L3", "R3"]
PICTURES = {"button": ["normal", "pressed"], "cross": ["normal", "up", "down", "left", "right", "upLeft", "upRight", "downLeft", "downRight"],
            "stick": ["well", "knob", "knobHeld"]}
SLOTS = 9
SHAPES = {"circle": 0, "ring": 1, "rect": 2, "polygon": 3, "line": 4}
PAINTS = {"item": 1, "light": 2, "dark": 3}
NONE = 0xFFFF


def rgb565(color, what):
    if not isinstance(color, str) or not re.fullmatch(r"#[0-9a-fA-F]{6}", color):
        raise ValueError(f"{what}: a color is #rrggbb, not {color!r}")
    v = int(color[1:], 16)
    r, g, b = v >> 16 & 255, v >> 8 & 255, v & 255
    return (r & 0xF8) << 8 | (g & 0xFC) << 3 | (b & 0xF8) >> 3


def number(v, what, least=-32768):
    if not isinstance(v, int) or isinstance(v, bool) or not least <= v <= 32767:
        raise ValueError(f"{what}: a whole number of tenths of a millimetre from {least}, not {v!r}")
    return v


def blob(layout):
    if not isinstance(layout, dict) or layout.get("format") != FORMAT:
        raise ValueError(f"not a layout of the pad of DGX-DI (format {FORMAT!r})")
    if not isinstance(layout.get("version"), int) or layout["version"] > VERSION:
        raise ValueError(f"a layout of version {layout.get('version')!r}: this tool reads up to {VERSION}")
    items = layout.get("items")
    if not isinstance(items, list) or not 1 <= len(items) <= ITEMS:
        raise ValueError(f"a layout has 1 to {ITEMS} items")
    item_data, shape_data, point_data, strings = bytearray(), bytearray(), bytearray(), bytearray()
    shapes = points = 0
    for k, item in enumerate(items):
        where = f"item {k + 1}"
        if not isinstance(item, dict):
            raise ValueError(f"{where}: an object, not {item!r}")
        kind = item.get("kind")
        if kind not in KINDS:
            raise ValueError(f"{where}: a kind of button, cross or stick, not {kind!r}")
        fx, fy = item.get("fromX", "start"), item.get("fromY", "start")
        if fx not in FROM or fy not in FROM:
            raise ValueError(f"{where}: fromX and fromY are start, middle or end")
        x, y, r = number(item.get("x", 0), where + ", x"), number(item.get("y", 0), where + ", y"), number(item.get("r"), where + ", r", 1)
        if kind == "button":
            if item.get("out") not in BITS:
                raise ValueError(f"{where}: a button's out is one of {' '.join(BITS)}, not {item.get('out')!r}")
            out = 1 << BITS.index(item["out"])
        elif kind == "stick":
            if item.get("out", "left") not in ("left", "right"):
                raise ValueError(f"{where}: a stick's out is left or right")
            out = 1 if item.get("out") == "right" else 0
        else:
            out = 0
        ways = (1 if item.get("four") else 0) | (2 if item.get("floating") else 0)
        color = rgb565(item["color"], where) if item.get("color") else 0
        pictures = item.get("pictures")
        slots = []
        if pictures is not None:
            if not isinstance(pictures, dict) or any(name not in PICTURES[kind] for name in pictures):
                raise ValueError(f"{where}: a {kind}'s pictures are {', '.join(PICTURES[kind])}")
        for name in PICTURES[kind] + [None] * (SLOTS - len(PICTURES[kind])):
            shapes_of = (pictures or {}).get(name, []) if name else []
            if not isinstance(shapes_of, list):
                raise ValueError(f"{where}, {name}: a picture is a list of shapes")
            slots.append((shapes, len(shapes_of)))
            for i, sh in enumerate(shapes_of):
                w = f"{where}, {name}, shape {i + 1}"
                if not isinstance(sh, dict):
                    raise ValueError(f"{w}: an object, not {sh!r}")
                kind_of = sh.get("shape")
                if kind_of not in SHAPES:
                    raise ValueError(f"{w}: a shape of {', '.join(SHAPES)}, not {kind_of!r}")
                c = sh.get("color", "item")
                paint, own = (PAINTS[c], 0) if c in PAINTS else (0, rgb565(c, w))
                pts = sh.get("points", []) if kind_of in ("polygon", "line") else []
                if kind_of == "polygon" and len(pts) < 3:
                    raise ValueError(f"{w}: a polygon of 3 points at least")
                if kind_of == "line" and len(pts) < 2:
                    raise ValueError(f"{w}: a line of 2 points at least")
                sx, sy = number(sh.get("x", 0), w + ", x"), number(sh.get("y", 0), w + ", y")
                sw = number(sh.get("width", 1) if kind_of == "ring" else sh.get("w", 0), w + ", w", 0)
                shh = number(sh.get("h", 0), w + ", h", 0)
                sr = number(sh.get("width", 1) if kind_of == "line" else sh.get("r", 0), w + ", r", 0)
                shape_data += struct.pack("<BBHhhhhhHHH", SHAPES[kind_of], paint, own, sx, sy, sw, shh, sr, points, len(pts), 0)
                for j, p in enumerate(pts):
                    if not (isinstance(p, list) and len(p) == 2):
                        raise ValueError(f"{w}, point {j + 1}: [x, y]")
                    point_data += struct.pack("<hh", number(p[0], w + ", a point's x"), number(p[1], w + ", a point's y"))
                points += len(pts)
                shapes += 1
                if shapes > 0xFFFF or points > 0xFFFF:
                    raise ValueError("more than 65535 shapes or points")
        item_data += struct.pack("<BBBBhhhHI", KINDS[kind], FROM[fx], FROM[fy], ways, x, y, r, color, out)
        for first, n in slots:
            item_data += struct.pack("<HH", first if n else 0, n)
        item_data += struct.pack("<BBH", 1 if pictures is not None else 0, 0, 0)
    name = layout.get("name") or None
    if name:
        strings += name.encode("utf-8") + b"\0"
    header = b"DGXP" + struct.pack("<BBHHHHH", VERSION, len(items), shapes, points, 0 if name else NONE, 0, 0)
    return bytes(header + item_data + shape_data + point_data + strings)


def ident(name):
    s = re.sub(r"[^a-z0-9]+", "_", (name or "pad").lower()).strip("_") or "pad"
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
    ap = argparse.ArgumentParser(description="A layout of the pad of DGX-DI (JSON of tools/pad-editor) as a blob.")
    ap.add_argument("--c", metavar="NAME", help="write a C array NAME[] and NAME_size, not a binary file")
    ap.add_argument("--builtin", metavar="OUT", help="write DGX-DI's own layouts of the JSON files given into OUT")
    ap.add_argument("files", nargs="+", help="layout.json out (or, with --builtin, the layouts)")
    a = ap.parse_args()
    try:
        if a.builtin:
            out = ["/*", " * The ready layouts of the pad of DGX-DI (dgx_di_pad.h): blobs made by tools/pad_blob.py of the JSON files in",
                   " * tools/pad-editor/examples. Change those (in the editor) and make this again:",
                   " *", " *     python3 tools/pad_blob.py --builtin src/dgx_di_pad_layouts.c tools/pad-editor/examples/[a-z]*.json",
                   " */", '#include "dgx_di_pad.h"', ""]
            for path in sorted(a.files, key=os.path.basename):
                layout = read(path)
                out.append(array("dgx_di_pad_layout_" + ident(layout.get("name")), blob(layout), os.path.basename(path)))
            with open(a.builtin, "w", encoding="utf-8") as f:
                f.write("\n".join(out))
            return
        if len(a.files) != 2:
            ap.error("a layout and where to write it")
        data = blob(read(a.files[0]))
        if a.c:
            text = "\n".join([f"/* The pad layout of {os.path.basename(a.files[0])}, made by tools/pad_blob.py of DGX-DI: dgx_di_pad_open(..., {a.c}, {a.c}_size) */",
                              "#include <stddef.h>", "#include <stdint.h>", "", array(a.c, data, os.path.basename(a.files[0]))])
            with open(a.files[1], "w", encoding="utf-8") as f:
                f.write(text)
        else:
            with open(a.files[1], "wb") as f:
                f.write(data)
    except (OSError, ValueError, json.JSONDecodeError) as e:
        sys.exit(f"pad_blob.py: {e}")


if __name__ == "__main__":
    main()
