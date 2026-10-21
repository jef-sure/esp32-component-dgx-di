// The editor's model of the pad checked: the places of the items against DGX-DI's own, on the ready layouts and several
// displays, and colors mixed as DGX mixes a layer, as test/host/pad_dump makes them (make -C test/host check-pad-js);
// the ready layouts as the editor writes them; the fingers as dgx_di_pad_take takes them; the templates (written into
// a directory, if one is given, for pad_blob.py to make blobs of them: make -C test/host check-templates).
//
//     node test.js pad-geometry.json [templates/]
const fs = require('fs');
const path = require('path');
const assert = require('assert');
const P = require('./Pad.js');

const failures = [];
const fail = text => failures.push(text);
const example = name => JSON.parse(fs.readFileSync(path.join(__dirname, 'examples', name + '.json'), 'utf8'));

const dump = JSON.parse(fs.readFileSync(process.argv[2], 'utf8'));
let items = 0;
for (const c of dump.places) {
    const pls = P.places(example(c.layout), c.display);
    const where = `${c.layout} on ${c.display.width}x${c.display.height} (${c.display.widthMm}x${c.display.heightMm} mm)`;
    if (pls.length !== c.items.length) {
        fail(`${where}: ${pls.length} items, the C's ${c.items.length}`);
        continue;
    }
    c.items.forEach(([x, y, r], k) => {
        const p = pls[k];
        if (p.x !== x || p.y !== y || p.r !== r) fail(`${where}, item ${k + 1}: ${p.x},${p.y} r ${p.r}, the C's ${x},${y} r ${r}`);
    });
    items += c.items.length;
}
let mixed = 0;
for (const [alpha, under, layer, out] of dump.mixed) {
    const got = P.mix(under, layer, P.weightOf(alpha));
    if (got !== out) fail(`mixed at ${alpha}: ${layer.toString(16)} over ${under.toString(16)} is ${got.toString(16)}, DGX's ${out.toString(16)}`);
    ++mixed;
}

// the ready layouts as the editor writes them; the classic one as the model has it; the themed ones of it
for (const name of ['classic', 'flat', 'twin']) {
    const text = fs.readFileSync(path.join(__dirname, 'examples', name + '.json'), 'utf8'), layout = JSON.parse(text);
    if (P.toJson(layout) !== text) fail(`examples/${name}.json: not as the editor writes it`);
    if (P.check(layout)) fail(`examples/${name}.json: ${P.check(layout)}`);
    for (const d of P.Displays) {
        const errors = P.problems(layout, d).filter(p => p.level === 'error');
        if (errors.length) fail(`examples/${name}.json on ${d.name}: ${errors.map(p => p.text).join('; ')}`);
    }
}
// the templates: of the format, read back the same, nothing wrong on the displays they are for (the mini one the CYD's
// too); DGX-DI's own as examples/ has them; made anew each time
for (const t of P.Templates) {
    const layout = P.template(t.name), text = P.toJson(layout);
    if (P.check(layout)) fail(`the template ${t.name}: ${P.check(layout)}`);
    if (P.toJson(JSON.parse(text)) !== text) fail(`the template ${t.name}: not read back the same`);
    for (const d of t.name === 'mini' ? P.Displays : P.Displays.slice(0, 2)) {
        const told = P.problems(layout, d);
        if (told.length) fail(`the template ${t.name} on ${d.name}: ${told.map(p => p.text).join('; ')}`);
    }
    if (t.own && text !== fs.readFileSync(path.join(__dirname, 'examples', t.name + '.json'), 'utf8')) fail(`the template ${t.name}: not as examples/${t.name}.json`);
    layout.items[0].x += 10;
    if (P.template(t.name).items[0].x === layout.items[0].x) fail(`the template ${t.name}: not made anew`);
    if (process.argv[3]) {
        fs.writeFileSync(path.join(process.argv[3], t.name + '.json'), text);
        // the blob as the editor makes it, for the Makefile to compare with pad_blob.py's
        fs.writeFileSync(path.join(process.argv[3], t.name + '.pad'), P.toBlob(JSON.parse(text)));
    }
}
// each theme of a template too, as the editor makes the blob: compared with pad_blob.py's by the Makefile
if (process.argv[3]) for (const name of Object.keys(P.Themes)) {
    const layout = P.themed(P.template('arcade'), name);
    layout.name = 'arcade-' + name.toLowerCase();
    fs.writeFileSync(path.join(process.argv[3], layout.name + '.json'), P.toJson(layout));
    fs.writeFileSync(path.join(process.argv[3], layout.name + '.pad'), P.toBlob(layout));
}
if (P.template('none').name !== 'classic') fail('no template of a name: not the classic one');

try {
    assert.deepStrictEqual(example('classic'), P.classic());
    const flat = P.themed(P.classic(), 'Flat');
    flat.name = 'flat';
    assert.deepStrictEqual(example('flat'), flat);
    // the fingers, as the C takes them
    const d = P.Displays[1], layout = P.classic(), pls = P.places(layout, d), cross = pls[0], a = pls[1];
    assert.strictEqual(P.itemAt(pls, a.x, a.y), 1);
    assert.strictEqual(P.itemAt(pls, 0, 0), -1);
    assert.strictEqual(P.hold(layout.items[1], a, a.x + Math.trunc(a.r * 14 / 10), a.y).buttons, P.bit('A'));
    assert.strictEqual(P.hold(layout.items[1], a, a.x + Math.trunc(a.r * 16 / 10), a.y).buttons, 0);
    const way = (dx, dy) => P.hold(layout.items[0], cross, cross.x + Math.trunc(dx * cross.r / 100), cross.y + Math.trunc(dy * cross.r / 100)).buttons;
    assert.strictEqual(way(10, 0), 0);
    assert.strictEqual(way(80, 0), P.bit('RIGHT'));
    assert.strictEqual(way(60, 60), P.bit('RIGHT') | P.bit('DOWN'));
    assert.strictEqual(way(-40, 40), P.bit('LEFT') | P.bit('DOWN'));
    assert.strictEqual(way(80, 35), P.bit('RIGHT'));
    const stick = { kind: 'stick', out: 'right', floating: true, r: 150 }, sp = { x: 500, y: 300, r: 100 };
    const h = P.hold(stick, sp, 650, 300, 550, 300);
    assert.deepStrictEqual(h.axes, { side: 'right', x: 32767, y: 0 });
    assert.strictEqual(h.state.knobX, 50);
    // a place dragged and given back: the same place
    for (const it of layout.items) {
        const p = P.places({ items: [it] }, d)[0], back = P.unplace(it, d, p.x, p.y);
        const again = P.places({ items: [{ ...it, ...back }] }, d)[0];
        assert.ok(Math.abs(again.x - p.x) <= 1 && Math.abs(again.y - p.y) <= 1, `${it.out || it.kind}: ${p.x},${p.y} back as ${again.x},${again.y}`);
    }
    // every theme a layout pad_blob.py takes: pictures of the kinds' names, shapes of the shapes
    for (const name of Object.keys(P.Themes)) {
        const t = P.themed(P.classic(), name);
        const errors = P.problems(t, d).filter(p => p.level === 'error');
        assert.ok(!errors.length, `${name}: ${errors.map(p => p.text).join('; ')}`);
    }
    // what is wrong told
    const bad = P.classic();
    bad.items[1].out = 'Z';
    bad.items[2].pictures = { normal: [{ shape: 'polygon', points: [[0, 0], [1, 1]] }], sideways: [] };
    const told = P.problems(bad, d).map(p => p.text).join('\n');
    assert.match(told, /out is the name of a bit/);
    assert.match(told, /3 points at least/);
    assert.match(told, /no picture sideways/);
    assert.ok(P.check({ format: 'other' }));
    // the blob: its header as pad_blob.py writes it; a layout the program would not open refused with what is wrong
    const blob = P.toBlob(layout);
    assert.strictEqual(Buffer.from(blob.slice(0, 4)).toString(), 'DGXP');
    assert.strictEqual(blob[5], 7);
    assert.strictEqual(blob.length, 16 + 7 * 56 + 'classic\0'.length);
    assert.throws(() => P.toBlob(bad), /out is one of/);
    assert.throws(() => P.toBlob({ ...layout, items: [{ kind: 'cross', r: 10, x: 1.5 }] }), /whole number/);
    assert.strictEqual(P.ident('My Pad 2'), 'my_pad_2');
    assert.strictEqual(P.ident('7 inch'), '_7_inch');
    assert.match(P.toC('pad_x', blob, 'x.json'), /const uint8_t pad_x\[\] = \{\n    0x44, 0x47, 0x58, 0x50/);
} catch (e) {
    fail(e.message);
}

if (failures.length) {
    console.error(failures.slice(0, 20).join('\n') + (failures.length > 20 ? `\n... ${failures.length - 20} more` : ''));
    process.exit(1);
}
console.log(`the pad editor's model: its places as the C's, ${items} items in ${dump.places.length} cases; ${mixed} colors mixed as DGX mixes them; the ready layouts as the editor writes them; ${P.Templates.length} templates; the blob`);
