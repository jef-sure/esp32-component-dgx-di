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
    if (process.argv[3]) fs.writeFileSync(path.join(process.argv[3], t.name + '.json'), text);
}
if (L.template('none').name !== 'digits') fail('no template of a name: not the code of digits');

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
console.log(`the editor's model: its geometry as the C's, ${keys} keys in ${dump.length} cases; the ready layouts as the editor writes them; ${L.Templates.length} templates`);
