"""dgx_di_keyboard_font() of project_include.cmake checked, run by CMake as a script with dgx_font_need a stub: of each
layout given, the letters it writes (as the keyboard draws the labels: a key's label, else its character and the one
with shift) and the request it makes of DGX for its font (millimetres, pixels of a layout of before, a font of DGX, a
font of the program's own); a layout naming no font, or not a layout, stops it.

    python3 check_keyboard_font.py <work dir> <layout.json>...
"""
import json
import os
import subprocess
import sys

HERE = os.path.dirname(os.path.abspath(__file__))


def run(work, layout, need, extra=()):
    os.makedirs(work, exist_ok=True)
    return subprocess.run(["cmake", "-D", f"LAYOUT={layout}", "-D", f"NEED={need}", *extra, "-P", os.path.join(HERE, "keyboard_font.cmake")],
                          cwd=work, capture_output=True, text=True)


def labels(layout):
    found = set()
    for page in layout["pages"]:
        for row in page["rows"]:
            for key in row:
                texts = [key["label"]] if key.get("label") else [key.get("char") or "", key.get("upper") or ""] if "char" in key else []
                found |= {c for t in texts for c in t}
    return found


def main(work, layouts):
    failures, checked = [], 0
    os.makedirs(work, exist_ok=True)
    for path in layouts:
        layout = json.load(open(path, encoding="utf-8"))
        name = os.path.splitext(os.path.basename(path))[0]
        for font, want in (({"family": "DejaVu Sans", "style": "Book", "postscript": "DejaVuSans", "mm": 4.5}, "SIZE;4.5mm;POSTSCRIPT;DejaVuSans;TEXTS;"),
                           ({"family": "DejaVu Sans", "style": "Book", "postscript": "DejaVuSans", "px": 30}, "SIZE;30px;POSTSCRIPT;DejaVuSans;TEXTS;"),
                           ({"dgx": "TerminusTTFMedium12"}, "DGX;TerminusTTFMedium12;TEXTS;")):
            layout["font"] = font
            given = os.path.join(work, name + ".json")
            json.dump(layout, open(given, "w", encoding="utf-8"), ensure_ascii=False)
            r = run(work, given, "keys")
            letters_file = os.path.join(work, "dgx_di", "keys.letters")
            if r.returncode:
                failures.append(f"{name} {font}: {r.stderr.strip()}")
                continue
            asked = open(os.path.join(work, "keys.need"), encoding="utf-8").read()
            if asked != f"keys;{want}{letters_file}":
                failures.append(f"{name} {font}: asked {asked!r}")
            got = set(open(letters_file, encoding="utf-8").read()) - {"\n"}
            if got != labels(layout):
                failures.append(f"{name}: letters {''.join(sorted(got ^ labels(layout)))} not as the labels")
            checked += 1
        # a font of the program's own instead
        r = run(work, given, "keys", ("-D", "FILE=fonts/My.c"))
        asked = open(os.path.join(work, "keys.need"), encoding="utf-8").read()
        if r.returncode or not asked.startswith("keys;FILE;fonts/My.c;TEXTS;"):
            failures.append(f"{name} FILE: {r.stderr.strip() or asked}")
        # written only when the letters change: the same layout again leaves the file as it was
        before = os.stat(letters_file).st_mtime_ns
        run(work, given, "keys")
        if os.stat(letters_file).st_mtime_ns != before:
            failures.append(f"{name}: its letters written again, the same")
    # a layout naming no font, a file not a layout: stopped, with why
    layout = json.load(open(layouts[0], encoding="utf-8"))
    layout.pop("font", None)
    bare = os.path.join(work, "bare.json")
    json.dump(layout, open(bare, "w", encoding="utf-8"))
    r = run(work, bare, "keys")
    if not r.returncode or "names no font" not in r.stderr:
        failures.append(f"a layout naming no font: {r.returncode} {r.stderr.strip()}")
    other = os.path.join(work, "other.json")
    json.dump({"format": "dgx-di-pad", "items": []}, open(other, "w"))
    r = run(work, other, "keys")
    if not r.returncode or "not a keyboard layout" not in r.stderr:
        failures.append(f"not a layout: {r.returncode} {r.stderr.strip()}")
    if failures:
        sys.exit("\n".join(failures))
    print(f"dgx_di_keyboard_font: the letters and the request of {checked} layouts' fonts, as DGX is to be asked")


if __name__ == "__main__":
    main(os.path.abspath(sys.argv[1]), sys.argv[2:])
