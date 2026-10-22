// The editor's model checked: its geometry against DGX-DI's own, on the ready layouts, as test/host/keyboard_dump makes
// it (make -C test/host check-keyboard-js); the layouts' files as the editor writes them; colors; what a key types;
// the templates (written into a directory, if one is given, for keyboard_blob.py: make -C test/host check-templates).
//
//     node test.js keyboard-geometry.json [templates/]
const fs = require('fs');
const path = require('path');
const assert = require('assert');
const L = require('./Layout.js');

const failures = [];
const fail = text => failures.push(text);
const example = name => JSON.parse(fs.readFileSync(path.join(__dirname, 'examples', name + '.json'), 'utf8'));

// the geometry, case by case
const dump = JSON.parse(fs.readFileSync(process.argv[2], 'utf8'));
let keys = 0;
for (const c of dump) {
    const layout = example(c.layout), band = L.place(layout, c.display, c.field);
    const where = `${c.layout} on ${c.display.width}x${c.display.height} (${c.display.widthMm}x${c.display.heightMm} mm)${c.field ? ' with a field at the bottom' : ''}`;
    if (!band || !c.band) {
        if (!band !== !c.band) fail(`${where}: the editor ${band ? 'places' : 'does not place'} it, the C ${c.band ? 'does' : 'does not'}`);
        continue;
    }
    for (const k of Object.keys(c.band))
        if (band[k] !== c.band[k]) fail(`${where}: ${k} ${band[k]}, the C's ${c.band[k]}`);
    c.pages.forEach((rects, p) => {
        const laid = L.lay(layout, band, p);
        if (laid.length !== rects.length) return fail(`${where}, page ${p + 1}: ${laid.length} keys, the C's ${rects.length}`);
        rects.forEach(([x, y, w, h], i) => {
            const k = laid[i];
            if (k.x !== x || k.y !== y || k.w !== w || k.h !== h) fail(`${where}, page ${p + 1}, key ${i + 1}: ${k.x},${k.y} ${k.w}x${k.h}, the C's ${x},${y} ${w}x${h}`);
        });
        keys += rects.length;
    });
}
if (!dump.length || !keys) fail('no geometry to compare');

// the ready layouts: written as the editor writes them, no problems on the displays of the boards
for (const name of ['digits', 'latin']) {
    const text = fs.readFileSync(path.join(__dirname, 'examples', name + '.json'), 'utf8'), layout = JSON.parse(text);
    if (L.toJson(layout) !== text) fail(`examples/${name}.json: not as the editor writes it`);
    if (L.check(layout)) fail(`examples/${name}.json: ${L.check(layout)}`);
    for (const display of L.Displays) {
        const problems = L.problems(layout, display);
        if (problems.length) fail(`examples/${name}.json on ${display.name}: ${problems.map(p => p.text).join('; ')}`);
    }
}

// the templates: of the format, read back the same, nothing wrong on the displays of the boards; DGX-DI's own as
// examples/ has them; made anew each time
for (const t of L.Templates) {
    const layout = L.template(t.name), text = L.toJson(layout);
    if (L.check(layout)) fail(`the template ${t.name}: ${L.check(layout)}`);
    if (L.toJson(JSON.parse(text)) !== text) fail(`the template ${t.name}: not read back the same`);
    for (const display of L.Displays) {
        const told = L.problems(layout, display);
        if (told.length) fail(`the template ${t.name} on ${display.name}: ${told.map(p => p.text).join('; ')}`);
    }
    if (t.own && text !== fs.readFileSync(path.join(__dirname, 'examples', t.name + '.json'), 'utf8')) fail(`the template ${t.name}: not as examples/${t.name}.json`);
    layout.pages[0].rows[0][0].char = '~';
    if (L.template(t.name).pages[0].rows[0][0].char === '~') fail(`the template ${t.name}: not made anew`);
    if (process.argv[3]) {
        fs.writeFileSync(path.join(process.argv[3], t.name + '.json'), text);
        fs.writeFileSync(path.join(process.argv[3], t.name + '.letters'), require('./Fonts.js').letters(L.template(t.name)));
    }
}
if (L.template('none').name !== 'digits') fail('no template of a name: not the code of digits');

// the fonts of DGX as DGX draws them: the bounds and the pixels of strings, and the Latin keyboard's labels in one of
// them, key by key (font_dump: node test.js geometry.json templates/ font-dump.json DGX/src/fonts)
let glyphs = 0;
if (process.argv[4]) {
    const F = require('./Fonts.js'), fonts = JSON.parse(fs.readFileSync(process.argv[4], 'utf8')), dir = process.argv[5];
    const fontOf = file => F.fromC(fs.readFileSync(path.join(dir, file), 'utf8'));
    // what draw() fills, pixel by pixel, row by row as the C lists them; of a rectangle only, if one is given
    const drawn = (font, x, y, text, rect) => {
        const out = [];
        F.draw({ fillRect: (px, py, w, h) => { for (let j = 0; j < h; ++j) for (let i = 0; i < w; ++i) out.push([px + i, py + j]); } }, font, x, y, text);
        const inside = ([px, py]) => !rect || (px >= rect[0] && py >= rect[1] && px < rect[0] + rect[2] && py < rect[1] + rect[3]);
        return JSON.stringify(out.filter(inside).sort((a, b) => a[1] - b[1] || a[0] - b[0]));
    };
    for (const f of fonts.fonts) {
        const font = fontOf(f.file);
        for (const t of f.strings) {
            const b = F.bounds(font, t.text);
            if (b.width !== t.width || b.top !== t.top || b.height !== t.height) fail(`${f.file} "${t.text}": bounds ${JSON.stringify(b)}, DGX's ${t.width} ${t.top} ${t.height}`);
            if (drawn(font, t.from[0], t.from[1], t.text) !== JSON.stringify(t.pixels)) fail(`${f.file} "${t.text}": its pixels not DGX's`);
            glyphs += t.pixels.length;
        }
    }
    for (const k of fonts.keyboards) {
        const layout = example(k.layout), band = L.place(layout, k.display, null), keys = L.lay(layout, band, 0), font = fontOf(k.font);
        if (!k.open || keys.length !== k.keys.length) fail(`the labels of ${k.layout} in ${k.font}: ${keys.length} keys, DGX's ${k.keys.length}`);
        else keys.forEach((key, i) => {
            const label = L.label(key.key, false), c = k.keys[i];
            if (!label) return; // a sign or the space: drawn by sign() of the C, compared by the geometry
            const at = F.labelAt(font, key, band.keyH, label);
            if (drawn(font, at.x, at.y, label, c.rect) !== JSON.stringify(c.pixels)) fail(`the label ${label} of ${k.layout} in ${k.font}: not where DGX-DI draws it, or not as`);
            glyphs += c.pixels.length;
        });
    }
}

// the fonts of files: the names and the letters of a TTF of the computer, if it has DejaVu Sans
{
    const F = require('./Fonts.js'), ttf = '/usr/share/fonts/truetype/dejavu/DejaVuSans.ttf';
    if (fs.existsSync(ttf)) {
        const data = fs.readFileSync(ttf), face = F.sfnt(data.buffer.slice(data.byteOffset, data.byteOffset + data.byteLength));
        if (face.names.family !== 'DejaVu Sans' || face.names.style !== 'Book' || face.names.postscript !== 'DejaVuSans') fail(`DejaVuSans.ttf: its names ${JSON.stringify(face.names)}`);
        if (!face.has(0x41) || !face.has(0x451) || !face.has(0xdf) || face.has(0xe000) || face.has(0x10ffff)) fail('DejaVuSans.ttf: the letters it has, not as told');
    }
    // millimetres as pixels as DGX counts them: across, half to even; a layout of before keeps its pixels
    const d10 = L.Displays[1], d7 = L.Displays[0];
    if (L.mmToPx(4, d10) !== 24 || L.mmToPx(4.5, d7) !== 30 || L.mmToPx(2.5, { width: 10, widthMm: 10 }) !== 2 || L.mmToPx(3.5, { width: 10, widthMm: 10 }) !== 4)
        fail(`mm as px: ${L.mmToPx(4, d10)} ${L.mmToPx(4.5, d7)} ${L.mmToPx(2.5, { width: 10, widthMm: 10 })} ${L.mmToPx(3.5, { width: 10, widthMm: 10 })}`);
    if (L.fontPx({ postscript: 'X', px: 30 }, d10) !== 30 || L.fontPx({ postscript: 'X', mm: 4 }, d10) !== 24) fail('the pixels of a layout\'s font');
    // what a layout wants of a font: its letters past ASCII, the keys whose letters a font has not
    const de = L.template('german');
    if (F.letters(de) !== 'ÄÖÜßäöü€') fail(`the letters of German: ${F.letters(de)}`);
    const ascii = F.lacking(de, cp => cp < 0x7f);
    if (ascii.map(x => x.chars.join('')).join(' ') !== 'üÜ öÖ äÄ ß €') fail(`German in ASCII: ${JSON.stringify(ascii)}`);
}

// the rest of the model
try {
    assert.strictEqual(L.rgb565('#ffffff'), 0xffff);
    assert.strictEqual(L.rgb565('#2b2f36'), 0x2966); // as DGX's dgx_rgb_to_16, the low bits dropped
    assert.strictEqual(L.hex565(0xffff), '#ffffff');
    assert.strictEqual(L.hex565(L.rgb565('#08ff00')), '#08ff00');
    assert.strictEqual(L.mix565(0x0000, 0xffff, 1, 1), 0xffff);
    const q = { char: 'q', upper: 'Q' }, shift = { action: 'shift' }, page = { page: 1, label: '123' }, space = { char: ' ', width: 20 };
    assert.strictEqual(L.typed(q, false), 0x71);
    assert.strictEqual(L.typed(q, true), 0x51);
    assert.strictEqual(L.label(q, true), 'Q');
    assert.strictEqual(L.label(shift, false), null);
    assert.strictEqual(L.label(space, false), null);
    assert.strictEqual(L.label(page, false), '123');
    assert.strictEqual(L.code(shift), L.SHIFT);
    assert.strictEqual(L.code(page), L.PAGE + 1);
    assert.strictEqual(L.code({ action: 'erase' }), 8);
    assert.deepStrictEqual(L.cleanKey({ char: 'a', upper: '', label: '', width: 4 }), { char: 'a' });
    // what is wrong told
    const bad = example('latin');
    bad.pages[0].rows[0].push(...Array(40).fill({ char: 'x' }));
    bad.pages[1].rows[3][0].page = 7;
    const told = L.problems(bad, L.Displays[0]).map(p => p.text).join('\n');
    assert.match(told, /at most 48 keys/);
    assert.match(told, /no page 8/);
    assert.ok(L.problems(L.starter(), L.Displays[0]).length === 0);
    assert.ok(L.check({ format: 'other' }));
} catch (e) {
    fail(e.message);
}

if (failures.length) {
    console.error(failures.slice(0, 20).join('\n') + (failures.length > 20 ? `\n... ${failures.length - 20} more` : ''));
    process.exit(1);
}
console.log(`the editor's model: its geometry as the C's, ${keys} keys in ${dump.length} cases; the ready layouts as the editor writes them; ${L.Templates.length} templates; ${glyphs} pixels of labels as DGX draws them`);
