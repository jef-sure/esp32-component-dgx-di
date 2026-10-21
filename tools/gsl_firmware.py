#!/usr/bin/env python3
"""
The firmware of a Silead GSL touch controller (GSL3680 and its kin) as data for dgx_di_gsl3680.

The chip keeps no firmware of its own: it is written into it over I2C at every start. Vendors give it as a C array of
records {register, word}: register 0xf0 picks a page, the rest write words into it. In every firmware seen, a page is
32 words at registers 0x00..0x7c in order, so the file keeps it as such:

    "GSLF", u16 pages, u16 width, u16 height (little-endian), then for each page: u8 page, 32 words little-endian (128
    bytes)

the pages in the order the array has them. A firmware not of that shape is refused. Width and height are the range of
the coordinates the firmware gives, x 0..width-1 and y 0..height-1 as the chip names its axes: 64 a channel of the
panel's sensor (the 10" Guition JC8012P4A1C: 26 x 14 channels, 1664x896). The array does not tell them; the vendor's
driver or a touch at the corners of the panel does.

    gsl_firmware.py header.h out.fw --range 1664x896     the array GSLX680_FW[] (or --array NAME) of a vendor's header
"""
import argparse
import re
import struct
import sys

PAGE_WORDS = 32


def records(text, array):
    start = text.find(array + "[]")
    if start < 0:
        raise ValueError(f"no array {array}[]")
    end = text.find("};", start)
    if end < 0:
        raise ValueError(f"{array}[] not closed with }};")
    body = text[start:end]
    return [(int(reg, 0), int(word, 0)) for reg, word in re.findall(r"\{\s*(0x[0-9a-fA-F]+|\d+)\s*,\s*(0x[0-9a-fA-F]+|\d+)\s*\}", body)]


def pages(recs):
    out = []
    for reg, word in recs:
        if reg == 0xf0:
            if word > 0xff:
                raise ValueError(f"page {word:#x} past a byte")
            out.append((word, []))
        elif not out:
            raise ValueError("a word before the first page")
        elif word > 0xffffffff:
            raise ValueError(f"page {out[-1][0]:#x}: the word {word:#x} at {reg:#x} past 32 bits")
        else:
            out[-1][1].append((reg, word))
    for page, words in out:
        if [reg for reg, _ in words] != list(range(0, 4 * PAGE_WORDS, 4)):
            raise ValueError(f"page {page:#x}: not {PAGE_WORDS} words in order from 0")
    return out


def blob(pgs, width, height):
    data = bytearray(b"GSLF" + struct.pack("<HHH", len(pgs), width, height))
    for page, words in pgs:
        data.append(page)
        data += struct.pack(f"<{PAGE_WORDS}I", *(word for _, word in words))
    return bytes(data)


def main():
    ap = argparse.ArgumentParser(description="A Silead GSL firmware array as pages for dgx_di_gsl3680.")
    ap.add_argument("header")
    ap.add_argument("out")
    ap.add_argument("--array", default="GSLX680_FW")
    ap.add_argument("--range", required=True, metavar="WxH", help="the range of the coordinates the firmware gives, e.g. 1664x896")
    a = ap.parse_args()
    m = re.fullmatch(r"(\d+)[xX](\d+)", a.range)
    if not m or not 0 < int(m[1]) <= 4096 or not 0 < int(m[2]) <= 4096:
        sys.exit(f"--range {a.range}: not WxH of 1..4096 each (12 bits)")
    try:
        recs = records(open(a.header, encoding="utf-8", errors="replace").read(), a.array)
        pgs = pages(recs)
    except (OSError, ValueError) as e:
        sys.exit(f"{a.header}: {e}")
    data = blob(pgs, int(m[1]), int(m[2]))
    open(a.out, "wb").write(data)
    print(f"{a.out}: {len(recs)} records, {len(pgs)} pages, {m[1]}x{m[2]}, {len(data)} bytes")


if __name__ == "__main__":
    main()
