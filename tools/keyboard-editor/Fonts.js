// The fonts of the labels as the editor shows them: the browser's own (the default), a font of the computer (Chrome and
// Edge list them by queryLocalFonts, once allowed), a font file (TTF, OTF, a collection), or a font of DGX (the C that
// DGX's font2c writes, or the JSON of DGX-GE's dgx_ge_font.py), drawn pixel for pixel as DGX draws it: font.c of DGX,
// dgx_font_find_glyph, dgx_font_string_bounds and dgx_font_char_to_screen, step for step. Of a font file the tables
// name and cmap are read: the names its file is found by again for the build (fontconfig), and the letters it has.
const KeyboardFonts = (() => {
    // --- a font of DGX: {name, size (its yAdvance), glyphs: code point → {w, h, xAdvance, xOffset, yOffset, bits},
    // missingAdvance (a code point it has no glyph of advances by the first glyph of the font)}

    const stripComments = s => s.replace(/\/\*[\s\S]*?\*\//g, '').replace(/\/\/[^\n]*/g, '');

    // the C of font2c: its bitmaps, glyphs, ranges and the fields of the font its function gives
    function fromC(text) {
        const bitmaps = /bitmaps\[\]\s*=\s*\{([\s\S]*?)\};/.exec(text);
        if (!bitmaps) throw new Error('no bitmaps[]: not a font of font2c');
        const bytes = Uint8Array.from(stripComments(bitmaps[1]).match(/0x[0-9a-fA-F]+/g) || [], h => parseInt(h, 16));
        const glyphs = /glyph_t\s+glyphs\[\]\s*=\s*\{([\s\S]*?)\};/.exec(text);
        if (!glyphs) throw new Error('no glyphs[]: not a font of font2c');
        const list = [...stripComments(glyphs[1]).matchAll(/\{\s*\{\s*(?:\.bitmap\s*=\s*)?bitmaps\s*\+\s*(\d+)\s*\}\s*,\s*(-?\d+)\s*,\s*(-?\d+)\s*,\s*(-?\d+)\s*,\s*(-?\d+)\s*,\s*(-?\d+)\s*\}/g)]
            .map(m => m.slice(1).map(Number));
        const ranges = /glyph_array_t\s+glyph_ranges\[\]\s*=\s*\{([\s\S]*?)\};/.exec(text);
        if (!ranges) throw new Error('no glyph_ranges[]: not a font of font2c');
        const spans = [...stripComments(ranges[1]).matchAll(/\{\s*(0x[0-9a-fA-F]+|\d+)\s*,\s*(0x[0-9a-fA-F]+|\d+)\s*,\s*glyphs\s*\+\s*(\d+)\s*\}/g)]
            .map(m => [Number(m[1]), Number(m[2]), Number(m[3])]);
        const head = /dgx_font_t\s*\*\s*(\w+)\s*\(/.exec(text);
        if (!head) throw new Error('no function giving the font');
        const tail = text.slice(head.index), field = n => (new RegExp(`\\.${n}\\s*=\\s*([-\\w.]+)`).exec(tail) || [])[1];
        const type = field('f_type') || 'DGX_FONT_BITMAP_LINES';
        if (type !== 'DGX_FONT_BITMAP_LINES' && type !== 'DGX_FONT_BITMAP_STREAM') throw new Error(`${head[1]}: ${type}, not of bitmaps: the keyboard draws no label of it`);
        const stream = type === 'DGX_FONT_BITMAP_STREAM', map = new Map();
        for (const [first, number, k] of spans)
            for (let i = 0; i < number; ++i) {
                const g = list[k + i];
                if (!g) throw new Error(`${head[1]}: the range from ${first.toString(16)} goes past its glyphs`);
                const [at, w, h, xAdvance, xOffset, yOffset] = g, bits = new Uint8Array(w * h), pitch = (w + 7) >> 3;
                for (let y = 0; y < h; ++y)
                    for (let x = 0; x < w; ++x) {
                        const bit = stream ? y * w + x : y * pitch * 8 + x;
                        bits[y * w + x] = (bytes[at + (bit >> 3)] || 0) >> (7 - (bit & 7)) & 1;
                    }
                map.set(first + i, { w, h, xAdvance, xOffset, yOffset, bits });
            }
        const firstGlyph = spans.length ? list[spans[0][2]] : null;
        return { name: head[1], size: Number(field('yAdvance')) || 0, glyphs: map, missingAdvance: firstGlyph ? firstGlyph[3] : 0 };
    }

    // the JSON of dgx_ge_font.py: its glyphs as rows of '#' and '.'
    function fromJson(json) {
        if (!json || typeof json.glyphs !== 'object') throw new Error('not a font of dgx_ge_font.py');
        if (json.f_type && json.f_type !== 'DGX_FONT_BITMAP_LINES' && json.f_type !== 'DGX_FONT_BITMAP_STREAM') throw new Error(`${json.name}: ${json.f_type}, not of bitmaps`);
        const map = new Map();
        for (const [cp, g] of Object.entries(json.glyphs)) {
            const bits = new Uint8Array(g.w * g.h);
            (g.rows || []).forEach((row, y) => { for (let x = 0; x < g.w; ++x) bits[y * g.w + x] = row[x] === '#' ? 1 : 0; });
            map.set(Number(cp), { w: g.w, h: g.h, xAdvance: g.xAdvance, xOffset: g.xOffset, yOffset: g.yOffset, bits });
        }
        return { name: json.name || 'font', size: json.yAdvance || 0, glyphs: map, missingAdvance: json.first_advance || 0 };
    }

    // dgx_font_string_bounds(): the width (the advances), the top of its ink from the baseline and the height of it
    function bounds(font, text) {
        let width = 0, top = 0, bottom = 0, first = true;
        for (const ch of text) {
            const g = font.glyphs.get(ch.codePointAt(0));
            width += g ? g.xAdvance : font.missingAdvance;
            if (!g) continue;
            if (first || g.yOffset < top) top = g.yOffset;
            if (first || g.h + g.yOffset > bottom) bottom = g.h + g.yOffset;
            first = false;
        }
        return { width, top, height: bottom - top };
    }

    // dgx_font_string_utf8_screen(): each glyph from x and its baseline y by its offsets, a run of pixels a rectangle
    function draw(ctx, font, x, y, text) {
        for (const ch of text) {
            const g = font.glyphs.get(ch.codePointAt(0));
            if (g)
                for (let by = 0; by < g.h; ++by)
                    for (let bx = 0; bx < g.w; ++bx) {
                        if (!g.bits[by * g.w + bx]) continue;
                        const run = bx;
                        while (bx + 1 < g.w && g.bits[by * g.w + bx + 1]) ++bx;
                        ctx.fillRect(x + g.xOffset + run, y + g.yOffset + by, bx - run + 1, 1);
                    }
            x += g ? g.xAdvance : font.missingAdvance;
        }
    }

    // where the keyboard puts a label of a key: draw_key() and baseline() of dgx_di_keyboard.c, its middle across, a
    // capital in its middle up and down
    function labelAt(font, rect, keyH, text) {
        const cap = bounds(font, 'H'), b = bounds(font, text);
        return { x: rect.x + Math.trunc(rect.w / 2) - Math.trunc(b.width / 2), y: rect.y + Math.trunc(keyH / 2) - cap.top - Math.trunc(cap.height / 2) };
    }

    // --- a font file: its names and the letters it has

    function sfnt(buffer, postscript) {
        const v = new DataView(buffer), tag = o => String.fromCharCode(v.getUint8(o), v.getUint8(o + 1), v.getUint8(o + 2), v.getUint8(o + 3));
        if (buffer.byteLength < 12) throw new Error('not a font file');
        const faces = tag(0) === 'ttcf' ? Array.from({ length: v.getUint32(8) }, (_, i) => v.getUint32(12 + 4 * i)) : [0];
        const read = base => {
            if (v.getUint32(base) !== 0x00010000 && tag(base) !== 'OTTO' && tag(base) !== 'true') throw new Error('not a font file of TrueType or OpenType');
            const tables = {};
            for (let i = 0; i < v.getUint16(base + 4); ++i) {
                const r = base + 12 + 16 * i;
                tables[tag(r)] = v.getUint32(r + 8);
            }
            return { names: names(v, tables.name), has: coverage(v, tables.cmap) };
        };
        const all = faces.map(read);
        return (postscript && all.find(f => f.names.postscript === postscript)) || all[0];
    }

    // the table name: the family and style as fontconfig names a face (the typographic ones where they are), the full
    // and the PostScript names; a record of Windows in English first, then any of Windows or Unicode, then of a Mac
    function names(v, at) {
        const out = {}, rank = {}, ids = { 1: 'family', 2: 'style', 4: 'full', 6: 'postscript', 16: 'typoFamily', 17: 'typoStyle' };
        if (at === undefined) return out;
        const count = v.getUint16(at + 2), strings = at + v.getUint16(at + 4);
        for (let i = 0; i < count; ++i) {
            const r = at + 6 + 12 * i, platform = v.getUint16(r), language = v.getUint16(r + 4), key = ids[v.getUint16(r + 6)];
            const length = v.getUint16(r + 8), from = strings + v.getUint16(r + 10);
            const score = platform === 3 ? (language === 0x409 ? 3 : 2) : platform === 0 ? 2 : platform === 1 && language === 0 ? 1 : 0;
            if (!key || !score || (rank[key] || 0) >= score) continue;
            let s = '';
            if (platform === 1) for (let k = 0; k < length; ++k) s += String.fromCharCode(v.getUint8(from + k));
            else for (let k = 0; k + 1 < length; k += 2) s += String.fromCharCode(v.getUint16(from + k));
            out[key] = s, rank[key] = score;
        }
        return { family: out.typoFamily || out.family, style: out.typoStyle || out.style, full: out.full, postscript: out.postscript };
    }

    // the table cmap: whether a code point has a glyph (a subtable of format 12 or 4, of Unicode); null: not told
    function coverage(v, at) {
        if (at === undefined) return null;
        let best = null, bestRank = 0;
        for (let i = 0; i < v.getUint16(at + 2); ++i) {
            const r = at + 4 + 8 * i, platform = v.getUint16(r), encoding = v.getUint16(r + 2), sub = at + v.getUint32(r + 4), format = v.getUint16(sub);
            const unicode = platform === 0 || (platform === 3 && (encoding === 1 || encoding === 10));
            const rank = !unicode ? 0 : format === 12 ? 2 : format === 4 ? 1 : 0;
            if (rank > bestRank) best = { sub, format }, bestRank = rank;
        }
        if (!best) return null;
        const { sub, format } = best;
        if (format === 12) {
            const groups = [];
            for (let i = 0; i < v.getUint32(sub + 12); ++i) groups.push([v.getUint32(sub + 16 + 12 * i), v.getUint32(sub + 20 + 12 * i)]);
            return cp => groups.some(([a, b]) => cp >= a && cp <= b);
        }
        const segX2 = v.getUint16(sub + 6), ends = sub + 14, starts = ends + segX2 + 2, deltas = starts + segX2, offsets = deltas + segX2;
        return cp => {
            if (cp > 0xffff) return false;
            for (let s = 0; s < segX2; s += 2) {
                if (cp > v.getUint16(ends + s)) continue;
                const start = v.getUint16(starts + s), ro = v.getUint16(offsets + s);
                if (cp < start) return false;
                if (!ro) return ((cp + v.getInt16(deltas + s)) & 0xffff) !== 0;
                return v.getUint16(offsets + s + ro + 2 * (cp - start)) !== 0;
            }
            return false;
        };
    }

    // --- what a layout's labels want of a font

    // the model of a layout: the page's, or Node's
    const model = () => typeof KeyboardLayout === 'object' ? KeyboardLayout : require('./Layout.js');

    // the keys whose labels (with shift and without) have letters the font has not: [{page, row, key, chars}]
    function lacking(layout, has) {
        const out = [];
        layout.pages.forEach((page, p) => page.rows.forEach((row, r) => row.forEach((key, k) => {
            const chars = new Set();
            for (const shift of [false, true]) {
                const text = model().label(key, shift);
                if (text) for (const ch of text) if (!has(ch.codePointAt(0))) chars.add(ch);
            }
            if (chars.size) out.push({ page: p, row: r, key: k, chars: [...chars] });
        })));
        return out;
    }

    // every letter the labels of a layout have past ASCII, sorted: what a font of it must have beside ASCII
    function letters(layout) {
        const found = new Set();
        for (const page of layout.pages)
            for (const row of page.rows)
                for (const key of row)
                    for (const shift of [false, true]) {
                        const text = model().label(key, shift);
                        if (text) for (const ch of text) if (ch.codePointAt(0) > 0x7e) found.add(ch);
                    }
        return [...found].sort((a, b) => a.codePointAt(0) - b.codePointAt(0)).join('');
    }

    return { fromC, fromJson, bounds, draw, labelAt, sfnt, lacking, letters };
})();

if (typeof module === 'object') module.exports = KeyboardFonts;
