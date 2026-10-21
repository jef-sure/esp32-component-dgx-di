// The model of a layout of DGX-DI's pad: what the editor edits and shows. Its file is JSON (toJson; the format is told
// in tools/pad_blob.py, which makes the blob dgx_di_pad_open() takes of it).
//
// The places of the items are integer, step for step as src/dgx_di_pad.c makes them (make); the shapes of pictures in
// pixels as its shape() puts them; the default look as its draw(); the layer mixed over the picture under it as DGX
// mixes it (dgx_bus_overlay_mix); the fingers taken as its dgx_di_pad_take(). A change there is a change here: test.js
// compares the places with the C's.
const PadLayout = (() => {
    const Format = { name: 'dgx-di-pad', version: 1 };
    const Kinds = ['button', 'cross', 'stick'];
    const Bits = ['LEFT', 'RIGHT', 'UP', 'DOWN', 'A', 'START', 'BACK', 'B', 'X', 'Y', 'HOME', 'LB', 'RB', 'LT', 'RT', 'L3', 'R3'];
    const Pictures = { button: ['normal', 'pressed'], cross: ['normal', 'up', 'down', 'left', 'right', 'upLeft', 'upRight', 'downLeft', 'downRight'], stick: ['well', 'knob', 'knobHeld'] };
    // a cross's corners: their pictures, the ways both held, the signs of their x and y
    const Corners = [['upLeft', 'UP', 'LEFT', -1, -1], ['upRight', 'UP', 'RIGHT', 1, -1], ['downLeft', 'DOWN', 'LEFT', -1, 1], ['downRight', 'DOWN', 'RIGHT', 1, 1]];
    const Shapes = ['circle', 'ring', 'rect', 'polygon', 'line'];
    const Paints = ['item', 'light', 'dark'];
    const Limits = { items: 124 };
    const bit = name => 1 << Bits.indexOf(name);
    // the bits in groups, as the editor shows them: the face buttons, the ways, the shoulders, the rest
    const BitGroups = [
        { name: 'face', bits: ['A', 'B', 'X', 'Y'] },
        { name: 'ways', bits: ['LEFT', 'RIGHT', 'UP', 'DOWN'], labels: { LEFT: '\u25c0 LEFT', RIGHT: '\u25b6 RIGHT', UP: '\u25b2 UP', DOWN: '\u25bc DOWN' } },
        { name: 'shoulders', bits: ['LB', 'RB', 'LT', 'RT'] },
        { name: 'others', bits: ['START', 'BACK', 'HOME', 'L3', 'R3'] }
    ];
    // colors to pick from: grey (the default), those of the classic pad, a stick's, and a few more
    const Palette = ['#5a5a5a', '#29cf29', '#de1818', '#2192ef', '#efc721', '#4a5a78', '#ef7b21', '#9c4fd6', '#21c7b5', '#e8e8e8', '#202020'];
    // an item's name in words: Button A, Cross, Left stick
    const nameOf = it => it.kind === 'button' ? `Button ${it.out || '?'}` : it.kind === 'cross' ? 'Cross' : `${it.out === 'right' ? 'Right' : 'Left'} stick`;

    // the displays DGX-DI was tried on, as they stand in the programs that run on them
    const Displays = [
        { name: '7" Guition JC1060P470', width: 1024, height: 600, widthMm: 154, heightMm: 86 },
        { name: '10.1" Guition JC8012P4A1C', width: 1280, height: 800, widthMm: 217, heightMm: 136 },
        { name: 'CYD (ESP32-2432S028R)', width: 320, height: 240, widthMm: 58, heightMm: 43 }
    ];

    // tenths of a millimetre in pixels along an axis of px pixels over mm millimetres (C's integer division)
    const pixels = (t, px, mm) => Math.trunc((t * px * 2 + (t >= 0 ? 10 * mm : -10 * mm)) / (20 * mm));
    const placeOf = (from, at, size) => from === 'end' ? size - 1 - at : from === 'middle' ? Math.trunc(size / 2) + at : at;
    const scale = d => ({
        x: t => pixels(t, d.width, d.widthMm),
        y: t => pixels(t, d.height, d.heightMm),
        r: t => Math.trunc((pixels(t, d.width, d.widthMm) + pixels(t, d.height, d.heightMm) + 1) / 2)
    });

    // each item in pixels of the display: make() of the C
    function places(layout, d) {
        const s = scale(d);
        return layout.items.map(it => {
            const r = s.r(it.r);
            return { x: placeOf(it.fromX, s.x(it.x || 0), d.width), y: placeOf(it.fromY, s.y(it.y || 0), d.height), r: r < 2 ? 2 : r };
        });
    }

    // a place in pixels back as tenths of a millimetre from the item's edges: what dragging it gives
    function unplace(it, d, px, py) {
        const back = (from, p, size, mm) => {
            const at = from === 'end' ? size - 1 - p : from === 'middle' ? p - Math.trunc(size / 2) : p;
            return Math.round(at * mm * 10 / size);
        };
        return { x: back(it.fromX, px, d.width, d.widthMm), y: back(it.fromY, py, d.height, d.heightMm) };
    }

    // colors: RGB565 as DGX makes it of #rrggbb (the low bits dropped), and back
    function rgb565(hex) {
        const v = parseInt(hex.slice(1), 16), r = v >> 16 & 255, g = v >> 8 & 255, b = v & 255;
        return (r & 0xf8) << 8 | (g & 0xfc) << 3 | (b & 0xf8) >> 3;
    }
    function hex565(c) {
        const r = c >> 11 & 31, g = c >> 5 & 63, b = c & 31;
        return '#' + ((r << 3 | r >> 2) << 16 | (g << 2 | g >> 4) << 8 | (b << 3 | b >> 2)).toString(16).padStart(6, '0');
    }
    const lighter = c => ((c >> 11 & 31) + 31 >> 1) << 11 | ((c >> 5 & 63) + 63 >> 1) << 5 | ((c & 31) + 31 >> 1);
    const darker = c => (c >> 11 & 31) >> 1 << 11 | (c >> 5 & 63) >> 1 << 5 | (c & 31) >> 1;
    // a quarter of the way to white: a cross's brackets at rest
    const softer = c => (3 * (c >> 11 & 31) + 31 >> 2) << 11 | (3 * (c >> 5 & 63) + 63 >> 2) << 5 | (3 * (c & 31) + 31 >> 2);
    const GREY = rgb565('#5a5a5a'), RIM = rgb565('#e1e1e1'), SIGN = 0xffff;
    const itemColor = it => it.color ? rgb565(it.color) : GREY;
    function shapeColor(it, sh) {
        const c = sh.color || 'item', item = itemColor(it);
        return c === 'item' ? item : c === 'light' ? lighter(item) : c === 'dark' ? darker(item) : rgb565(c);
    }

    // the layer over what lies under it: dgx_bus_overlay_mix of DGX, its weight of alpha in 32 steps
    const weightOf = alpha => Math.trunc((alpha * 32 + 127) / 255);
    const spread = c => ((c | c << 16) >>> 0) & 0x07e0f81f;
    function mix(under, layer, weight) {
        const s = Math.trunc((spread(under) * (32 - weight) + spread(layer) * weight) / 32) & 0x07e0f81f;
        return (s | s >>> 16) & 0xffff;
    }

    // --- drawing on a canvas: the default look as draw() of the C, the pictures as its shape()

    const arm = r => Math.trunc(r * 7 / 20);
    function circle(ctx, x, y, r) {
        ctx.beginPath();
        ctx.arc(x + 0.5, y + 0.5, r + 0.5, 0, 2 * Math.PI);
        ctx.fill();
    }
    function outline(ctx, x, y, r) {
        ctx.lineWidth = 1;
        ctx.beginPath();
        ctx.arc(x + 0.5, y + 0.5, r, 0, 2 * Math.PI);
        ctx.stroke();
    }
    function triangle(ctx, x0, y0, x1, y1, x2, y2) {
        ctx.beginPath();
        ctx.moveTo(x0 + 0.5, y0 + 0.5), ctx.lineTo(x1 + 0.5, y1 + 0.5), ctx.lineTo(x2 + 0.5, y2 + 0.5);
        ctx.closePath();
        ctx.fill();
    }
    function rounded(ctx, x, y, w, h, r) {
        r = Math.min(r, Math.trunc(w / 2), Math.trunc(h / 2));
        if (r < 1) return ctx.fillRect(x, y, w, h);
        ctx.fillRect(x + r, y, w - 2 * r, h);
        ctx.fillRect(x, y + r, r, h - 2 * r);
        ctx.fillRect(x + w - r, y + r, r, h - 2 * r);
        for (const [ox, oy] of [[x + r, y + r], [x + w - 1 - r, y + r], [x + r, y + h - 1 - r], [x + w - 1 - r, y + h - 1 - r]]) circle(ctx, ox, oy, r);
    }
    // a line from point to point by a pen of w, round where it ends and turns
    function stroke(ctx, pts, w) {
        ctx.lineWidth = w, ctx.lineCap = 'round', ctx.lineJoin = 'round';
        ctx.beginPath();
        pts.forEach(([px, py], i) => i ? ctx.lineTo(px + 0.5, py + 0.5) : ctx.moveTo(px + 0.5, py + 0.5));
        ctx.stroke();
    }
    // a rectangle from corner to corner, both in it (bar() of the C)
    const bar = (ctx, x0, y0, x1, y1) => ctx.fillRect(Math.min(x0, x1), Math.min(y0, y1), Math.abs(x1 - x0) + 1, Math.abs(y1 - y0) + 1);
    // the four ways along the axes: a cross's arms, the buttons of a way
    const WayOf = [['LEFT', -1, 0], ['RIGHT', 1, 0], ['UP', 0, -1], ['DOWN', 0, 1]];
    // a chevron: its point d from x, y the way dx, dy, its wings e back and e aside (chevron() of the C)
    function chevron(ctx, x, y, dx, dy, d, e, w) {
        const px = x + dx * d, py = y + dy * d, bx = px - dx * e, by = py - dy * e;
        stroke(ctx, [[bx - dy * e, by + dx * e], [px, py], [bx + dy * e, by - dx * e]], w);
    }
    // a cross's arm held the way dx, dy: from its end to the square of the middle, its end round (arm_held() of the C)
    function heldArm(ctx, x, y, dx, dy, inner, end, ri) {
        const x0 = dx < 0 ? x - end : dx > 0 ? x + inner + 1 : x - inner, y0 = dy < 0 ? y - end : dy > 0 ? y + inner + 1 : y - inner;
        const w = dx ? end - inner : 2 * inner + 1, h = dy ? end - inner : 2 * inner + 1;
        rounded(ctx, x0, y0, w, h, ri);
        if (ri) ctx.fillRect(dx < 0 ? x - inner - ri : x0, dy < 0 ? y - inner - ri : y0, dx ? ri : w, dy ? ri : h);
    }
    const color = c => hex565(c);

    // an item as DGX-DI draws it by default (draw() of the C), as it is now: a button down, a cross's ways, a stick's knob
    function defaultLook(ctx, it, pl, state) {
        const { x, y, r } = pl, plain = color(itemColor(it)), held = color(lighter(itemColor(it)));
        const s = state || {};
        if (it.kind === 'button') {
            ctx.fillStyle = s.down ? held : plain;
            circle(ctx, x, y, r);
            ctx.strokeStyle = color(RIM);
            outline(ctx, x, y, r), outline(ctx, x, y, r - 1);
            const q = Math.trunc(r / 3), pen = Math.max(Math.trunc(r / 12), 3);
            ctx.fillStyle = ctx.strokeStyle = color(SIGN);
            if (it.out === 'START') triangle(ctx, x - q, y - q, x - q, y + q, x + q, y);
            if (it.out === 'BACK') triangle(ctx, x + q, y - q, x + q, y + q, x - q, y);
            if (it.out === 'HOME') ctx.fillRect(x - q, y - q, 2 * q + 1, 2 * q + 1);
            // a button of a way: its chevron, as a cross's
            for (const [way, dx, dy] of WayOf) if (it.out === way) chevron(ctx, x, y, dx, dy, Math.trunc(r * 15 / 100), Math.trunc(r * 3 / 10), pen);
        } else if (it.kind === 'cross') {
            // as the flat skin of Delta has one: on the plate, darker, a plus with a light rim, an arm held lighter, a
            // chevron on each, white while held, a ring round the middle; a bracket in each corner, white when that
            // diagonal is held (none on four ways)
            const a = arm(r), len = Math.trunc(r * 92 / 100), t = Math.max(Math.trunc(r / 45), 2), pen = Math.max(Math.trunc(r / 22), 3);
            const round = Math.trunc(a * 45 / 100), ri = Math.max(round - t, 0), inner = a - t, end = len - t, middle = Math.trunc(r * 3 / 20);
            const w = s.ways || 0;
            ctx.fillStyle = color(darker(itemColor(it)));
            circle(ctx, x, y, r);
            if (!it.four) {
                const A = Math.trunc(r * 60 / 100), L = Math.trunc(r * 13 / 100);
                for (const [, v, u, sx, sy] of Corners) {
                    const both = bit(v) | bit(u), px = x + sx * A, py = y + sy * A, lo = Math.trunc(pen / 2), hi = pen - 1 - lo;
                    ctx.fillStyle = color((w & both) === both ? SIGN : softer(itemColor(it)));
                    bar(ctx, px - sx * L, py - lo, px + (sx > 0 ? hi : -lo), py + hi);
                    bar(ctx, px - lo, py - sy * L, px + hi, py + (sy > 0 ? hi : -lo));
                }
            }
            ctx.fillStyle = color(RIM);
            rounded(ctx, x - len, y - a, 2 * len + 1, 2 * a + 1, round), rounded(ctx, x - a, y - len, 2 * a + 1, 2 * len + 1, round);
            ctx.fillStyle = plain;
            rounded(ctx, x - end, y - inner, 2 * end + 1, 2 * inner + 1, ri), rounded(ctx, x - inner, y - end, 2 * inner + 1, 2 * end + 1, ri);
            ctx.fillStyle = held;
            for (const [way, dx, dy] of WayOf) if (w & bit(way)) heldArm(ctx, x, y, dx, dy, inner, end, ri);
            for (const [way, dx, dy] of WayOf) {
                ctx.strokeStyle = color(w & bit(way) ? SIGN : RIM);
                chevron(ctx, x, y, dx, dy, Math.trunc(len * 78 / 100), Math.trunc(a * 42 / 100), pen);
            }
            ctx.strokeStyle = color(RIM), ctx.lineWidth = t;
            ctx.beginPath();
            ctx.arc(x + 0.5, y + 0.5, middle - Math.trunc(t / 2), 0, 2 * Math.PI);
            ctx.stroke();
        } else {
            const kx = x + (s.knobX || 0), ky = y + (s.knobY || 0), half = Math.trunc(r / 2);
            ctx.fillStyle = plain;
            circle(ctx, x, y, r);
            ctx.strokeStyle = color(RIM);
            outline(ctx, x, y, r);
            ctx.fillStyle = s.down ? held : plain;
            circle(ctx, kx, ky, half);
            outline(ctx, kx, ky, half);
        }
    }

    function shape(ctx, it, sh, cx, cy, s) {
        const c = color(shapeColor(it, sh)), x = cx + s.x(sh.x || 0), y = cy + s.y(sh.y || 0);
        ctx.fillStyle = ctx.strokeStyle = c;
        if (sh.shape === 'circle') circle(ctx, x, y, s.r(sh.r || 0));
        else if (sh.shape === 'ring') {
            const r = s.r(sh.r || 0), w = Math.min(Math.max(s.r(sh.width || 1), 1), r);
            ctx.lineWidth = w;
            ctx.beginPath();
            ctx.arc(x + 0.5, y + 0.5, r - Math.trunc(w / 2), 0, 2 * Math.PI);
            ctx.stroke();
        } else if (sh.shape === 'rect') {
            const w = s.x(sh.w || 0), h = s.y(sh.h || 0);
            rounded(ctx, x - Math.trunc(w / 2), y - Math.trunc(h / 2), w, h, s.r(sh.r || 0));
        } else if (sh.shape === 'polygon' || sh.shape === 'line') {
            const pts = (sh.points || []).map(([px, py]) => [cx + s.x(px) + 0.5, cy + s.y(py) + 0.5]);
            if (pts.length < 2) return;
            ctx.beginPath();
            ctx.moveTo(...pts[0]);
            for (const p of pts.slice(1)) ctx.lineTo(...p);
            if (sh.shape === 'polygon') ctx.closePath(), ctx.fill();
            else {
                ctx.lineWidth = Math.max(s.r(sh.width || 1), 1);
                ctx.lineCap = ctx.lineJoin = 'round';
                ctx.stroke();
                ctx.lineCap = 'butt', ctx.lineJoin = 'miter';
            }
        }
    }

    function picture(ctx, it, shapes, cx, cy, s) {
        for (const sh of shapes || []) shape(ctx, it, sh, cx, cy, s);
    }

    // an item as it is now (state: down, ways, knobX, knobY), drawn: by its pictures, or as by default
    function drawItem(ctx, it, pl, d, state) {
        const st = state || {}, s = scale(d);
        if (!it.pictures) return defaultLook(ctx, it, pl, st);
        const p = it.pictures;
        if (it.kind === 'button') picture(ctx, it, st.down ? p.pressed : p.normal, pl.x, pl.y, s);
        else if (it.kind === 'cross') {
            picture(ctx, it, p.normal, pl.x, pl.y, s);
            for (const way of ['up', 'down', 'left', 'right'])
                if ((st.ways || 0) & bit(way.toUpperCase())) picture(ctx, it, p[way], pl.x, pl.y, s);
            for (const [name, v, u] of Corners) {
                const both = bit(v) | bit(u);
                if (((st.ways || 0) & both) === both) picture(ctx, it, p[name], pl.x, pl.y, s);
            }
        } else {
            picture(ctx, it, p.well, pl.x, pl.y, s);
            picture(ctx, it, st.down ? p.knobHeld : p.knob, pl.x + (st.knobX || 0), pl.y + (st.knobY || 0), s);
        }
    }

    // --- fingers, as dgx_di_pad_take() takes them

    // the item a point is on: the first whose circle it is in; -1 none
    function itemAt(pls, x, y) {
        return pls.findIndex(p => (x - p.x) ** 2 + (y - p.y) ** 2 <= p.r * p.r);
    }
    function ways(dx, dy, r, four) {
        if ((dx * dx + dy * dy) * 400 < r * r * 9) return 0;
        const across = dx > 0 ? bit('RIGHT') : bit('LEFT'), along = dy > 0 ? bit('DOWN') : bit('UP');
        const ax = Math.abs(dx), ay = Math.abs(dy), a = arm(r);
        if (four || ax <= a || ay <= a) return ax >= ay ? across : along;
        return across | along;
    }
    // a finger of an item at x, y (down at downX, downY): its state, the buttons, the axes it makes
    function hold(it, pl, x, y, downX, downY) {
        let dx = x - pl.x, dy = y - pl.y;
        if (it.kind === 'button') {
            const held = 4 * (dx * dx + dy * dy) <= 9 * pl.r * pl.r;
            return { state: { down: held }, buttons: held ? bit(it.out) : 0, axes: null };
        }
        if (it.kind === 'cross') {
            const w = ways(dx, dy, pl.r, it.four);
            return { state: { ways: w }, buttons: w, axes: null };
        }
        if (it.floating) dx = x - downX, dy = y - downY;
        const d = Math.hypot(dx, dy);
        if (d > pl.r) dx = Math.round(dx * pl.r / d), dy = Math.round(dy * pl.r / d);
        const axis = v => Math.max(-32767, Math.min(32767, Math.round(v * 32767 / pl.r)));
        return { state: { down: true, knobX: Math.trunc(dx / 2), knobY: Math.trunc(dy / 2) }, buttons: 0, axes: { side: it.out === 'right' ? 'right' : 'left', x: axis(dx), y: axis(dy) } };
    }

    // --- themes: pictures for every item, of its kind, size and color

    const round = v => Math.round(v);
    const poly = pts => pts.map(([x, y]) => [round(x), round(y)]);
    function signs(it, r, colorOf) {
        const q = r / 3;
        if (it.out === 'START') return [{ shape: 'polygon', points: poly([[-q, -q], [-q, q], [q, 0]]), color: colorOf }];
        if (it.out === 'BACK') return [{ shape: 'polygon', points: poly([[q, -q], [q, q], [-q, 0]]), color: colorOf }];
        if (it.out === 'HOME') return [{ shape: 'rect', x: 0, y: 0, w: round(2 * q), h: round(2 * q), r: 0, color: colorOf }];
        const way = WayOf.find(([name]) => name === it.out);
        if (way) return [vee(way[1], way[2], r * 0.15, r * 0.3, Math.max(round(r / 12), 1), colorOf)];
        return [];
    }
    // a chevron as a line: its point d from the middle the way dx, dy, its wings e back and e aside
    function vee(dx, dy, d, e, width, colorOf) {
        const px = dx * d, py = dy * d, bx = px - dx * e, by = py - dy * e;
        return { shape: 'line', points: poly([[bx - dy * e, by + dx * e], [px, py], [bx + dy * e, by - dx * e]]), width, color: colorOf };
    }
    // a cross as the default look has it, after the flat skin of Delta: its arms to 0.92 of its radius, a chevron on
    // each, a bracket in each corner, a ring round the middle where a finger takes no way
    const LEN = 0.92, Ways = { left: [-1, 0], right: [1, 0], up: [0, -1], down: [0, 1] };
    const pen = r => Math.max(round(r / 22), 1);
    const armChevron = (r, way, colorOf, len = r * LEN) => vee(...Ways[way], len * 0.78, r * 0.35 * 0.42, pen(r), colorOf);
    const armChevrons = (r, colorOf, len) => ['left', 'right', 'up', 'down'].map(way => armChevron(r, way, colorOf, len));
    // a bracket: its two legs along the axes, back from its corner, square
    function bracket(r, sx, sy, colorOf) {
        const A = r * 0.6, L = r * 0.13, w = pen(r), along = round(L + w / 2), mid = A - L / 2 + w / 4;
        return [{ shape: 'rect', x: round(sx * mid), y: round(sy * A), w: along, h: w, r: 0, color: colorOf }, { shape: 'rect', x: round(sx * A), y: round(sy * mid), w, h: along, r: 0, color: colorOf }];
    }
    const middleRing = (r, colorOf) => ({ shape: 'ring', x: 0, y: 0, r: round(r * 0.15), width: Math.max(round(r / 45), 1), color: colorOf });
    // a cross's plate and its corners at rest; its corners held, lit
    function plate(it, colorOf, cornerColor) {
        return [{ shape: 'circle', x: 0, y: 0, r: it.r, color: colorOf }, ...(it.four ? [] : Corners.flatMap(([, , , sx, sy]) => bracket(it.r, sx, sy, cornerColor)))];
    }
    function corners(it, p, colorOf) {
        if (!it.four) for (const [name, , , sx, sy] of Corners) p[name] = bracket(it.r, sx, sy, colorOf);
        return p;
    }
    function crossArms(r, a, colorOf) {
        return [{ shape: 'rect', x: 0, y: 0, w: round(2 * r), h: round(2 * a), r: round(a / 3), color: colorOf }, { shape: 'rect', x: 0, y: 0, w: round(2 * a), h: round(2 * r), r: round(a / 3), color: colorOf }];
    }
    // an arm held: from its end to the square of the middle, its end round, its inner side straight
    function armHeld(r, a, way, colorOf) {
        const len = r - a, mid = a + len / 2, k = a / 3, inner = a + k / 2;
        const at = { up: [0, -mid, 2 * a, len], down: [0, mid, 2 * a, len], left: [-mid, 0, len, 2 * a], right: [mid, 0, len, 2 * a] }[way];
        const sq = { up: [0, -inner, 2 * a, k], down: [0, inner, 2 * a, k], left: [-inner, 0, k, 2 * a], right: [inner, 0, k, 2 * a] }[way];
        const rect = ([x, y, w, h], rr) => ({ shape: 'rect', x: round(x), y: round(y), w: round(w), h: round(h), r: rr, color: colorOf });
        return [rect(at, round(a / 3)), rect(sq, 0)];
    }
    const Themes = {
        Default: () => undefined, // as DGX-DI draws by default
        Flat(it) {
            const r = it.r;
            if (it.kind === 'button') return { normal: [{ shape: 'circle', x: 0, y: 0, r, color: 'item' }, ...signs(it, r, '#ffffff')], pressed: [{ shape: 'circle', x: 0, y: 0, r, color: 'light' }, ...signs(it, r, '#ffffff')] };
            if (it.kind === 'cross') {
                const a = r * 0.35, len = r * LEN, p = { normal: [...plate(it, 'dark', 'light'), ...crossArms(len, a, 'item'), ...armChevrons(r, '#ffffff', len), middleRing(r, 'dark')] };
                for (const w of ['up', 'down', 'left', 'right']) p[w] = [...armHeld(len, a, w, 'light'), armChevron(r, w, '#ffffff', len)];
                return corners(it, p, '#ffffff');
            }
            return { well: [{ shape: 'circle', x: 0, y: 0, r, color: 'dark' }], knob: [{ shape: 'circle', x: 0, y: 0, r: round(r / 2), color: 'item' }], knobHeld: [{ shape: 'circle', x: 0, y: 0, r: round(r / 2), color: 'light' }] };
        },
        Glass(it) {
            const r = it.r, shine = { shape: 'circle', x: round(-r * 0.3), y: round(-r * 0.32), r: round(r * 0.28), color: 'light' };
            if (it.kind === 'button') return {
                normal: [{ shape: 'circle', x: 0, y: 0, r, color: 'dark' }, { shape: 'circle', x: 0, y: 0, r: round(r * 0.84), color: 'item' }, shine, ...signs(it, r, '#ffffff')],
                pressed: [{ shape: 'circle', x: 0, y: 0, r, color: 'item' }, { shape: 'circle', x: 0, y: 0, r: round(r * 0.84), color: 'light' }, ...signs(it, r, '#ffffff')]
            };
            if (it.kind === 'cross') {
                const a = r * 0.35, len = r * (LEN - 0.06), p = { normal: [...plate(it, 'dark', 'light'), ...crossArms(r * LEN, a + r * 0.06, 'dark'), ...crossArms(len, a, 'item'), ...armChevrons(r, '#ffffff', len)] };
                for (const w of ['up', 'down', 'left', 'right']) p[w] = [...armHeld(len, a, w, 'light'), armChevron(r, w, '#ffffff', len)];
                return corners(it, p, '#ffffff');
            }
            const k = round(r / 2);
            return {
                well: [{ shape: 'circle', x: 0, y: 0, r, color: 'dark' }, { shape: 'ring', x: 0, y: 0, r, width: round(r * 0.08), color: 'item' }],
                knob: [{ shape: 'circle', x: 0, y: 0, r: k, color: 'item' }, { shape: 'circle', x: round(-k * 0.3), y: round(-k * 0.32), r: round(k * 0.3), color: 'light' }],
                knobHeld: [{ shape: 'circle', x: 0, y: 0, r: k, color: 'light' }]
            };
        },
        Outline(it) {
            const r = it.r, w = Math.max(round(r * 0.12), 4);
            if (it.kind === 'button') return { normal: [{ shape: 'ring', x: 0, y: 0, r, width: w, color: 'item' }, ...signs(it, r, 'item')], pressed: [{ shape: 'circle', x: 0, y: 0, r, color: 'item' }, ...signs(it, r, '#ffffff')] };
            if (it.kind === 'cross') {
                const a = r * 0.35, ar = round(a), rr = round(r * LEN);
                const edge = [[-ar, -rr], [ar, -rr], [ar, -ar], [rr, -ar], [rr, ar], [ar, ar], [ar, rr], [-ar, rr], [-ar, ar], [-rr, ar], [-rr, -ar], [-ar, -ar], [-ar, -rr]];
                const ring = { shape: 'ring', x: 0, y: 0, r: round(r), width: Math.max(round(w / 2), 2), color: 'item' };
                const p = { normal: [ring, { shape: 'line', points: edge, width: w, color: 'item' }, ...armChevrons(r, 'item'), ...(it.four ? [] : Corners.flatMap(([, , , sx, sy]) => bracket(r, sx, sy, 'item')))] };
                for (const way of ['up', 'down', 'left', 'right']) p[way] = [...armHeld(r * LEN, a, way, 'item'), armChevron(r, way, '#ffffff')];
                return corners(it, p, '#ffffff');
            }
            const k = round(r / 2);
            return { well: [{ shape: 'ring', x: 0, y: 0, r, width: w, color: 'item' }], knob: [{ shape: 'ring', x: 0, y: 0, r: k, width: w, color: 'item' }], knobHeld: [{ shape: 'circle', x: 0, y: 0, r: k, color: 'item' }] };
        },
        Neon(it) {
            const r = it.r, w = Math.max(round(r * 0.1), 4);
            const glow = (rr, color) => [{ shape: 'ring', x: 0, y: 0, r: rr, width: w * 2, color: 'dark' }, { shape: 'ring', x: 0, y: 0, r: round(rr - w / 2), width: w, color }];
            if (it.kind === 'button') return { normal: [{ shape: 'circle', x: 0, y: 0, r, color: '#000000' }, ...glow(r, 'item'), ...signs(it, r, 'light')], pressed: [{ shape: 'circle', x: 0, y: 0, r, color: 'dark' }, ...glow(r, 'light'), ...signs(it, r, '#ffffff')] };
            if (it.kind === 'cross') {
                const a = r * 0.35, len = r * LEN, p = { normal: [...plate(it, '#000000', 'item'), ...glow(r, 'dark').slice(0, 1), ...crossArms(len, a, '#000000'), ...armChevrons(r, 'light', len)] };
                for (const way of ['up', 'down', 'left', 'right']) p[way] = [...armHeld(len, a, way, 'item'), armChevron(r, way, '#ffffff', len)];
                return corners(it, p, 'light');
            }
            const k = round(r / 2);
            return { well: [{ shape: 'circle', x: 0, y: 0, r, color: '#000000' }, ...glow(r, 'item')], knob: [{ shape: 'circle', x: 0, y: 0, r: k, color: 'item' }], knobHeld: [{ shape: 'circle', x: 0, y: 0, r: k, color: 'light' }] };
        }
    };
    function themed(layout, name) {
        const out = JSON.parse(JSON.stringify(layout));
        for (const it of out.items) {
            const p = Themes[name](it);
            if (p) it.pictures = p;
            else delete it.pictures;
        }
        return out;
    }

    // a picture's shapes made a size larger or smaller (an item's radius changed)
    function scaled(shapes, k) {
        const m = v => Math.round(v * k);
        return (shapes || []).map(sh => {
            const out = { ...sh };
            for (const f of ['x', 'y', 'r', 'w', 'h', 'width']) if (out[f] !== undefined) out[f] = f === 'width' ? Math.max(1, m(out[f])) : m(out[f]);
            if (out.points) out.points = out.points.map(([x, y]) => [m(x), m(y)]);
            return out;
        });
    }

    // --- the file

    const ITEM_FIELDS = ['kind', 'fromX', 'fromY', 'x', 'y', 'r', 'out', 'four', 'floating', 'color'];
    const SHAPE_FIELDS = ['shape', 'x', 'y', 'r', 'w', 'h', 'width', 'points', 'color'];
    const inline = (value, fields) => '{' + fields.filter(f => value[f] !== undefined && value[f] !== false && value[f] !== '').map(f => JSON.stringify(f) + ': ' + JSON.stringify(value[f]).replace(/,/g, ', ')).join(', ') + '}';
    // the layout as the file of the editor: an item a line, each picture a line
    function toJson(layout) {
        const lines = ['{', `    "format": ${JSON.stringify(Format.name)},`, `    "version": ${Format.version},`, `    "name": ${JSON.stringify(layout.name || '')},`, '    "items": ['];
        layout.items.forEach((it, k) => {
            const head = inline(it, ITEM_FIELDS), last = k === layout.items.length - 1 ? '' : ',';
            if (!it.pictures) return lines.push('        ' + head + last);
            lines.push('        ' + head.slice(0, -1) + ', "pictures": {');
            const names = Pictures[it.kind].filter(n => it.pictures[n]);
            names.forEach((n, i) => lines.push(`            ${JSON.stringify(n)}: [` + it.pictures[n].map(sh => inline(sh, SHAPE_FIELDS)).join(', ') + ']' + (i < names.length - 1 ? ',' : '')));
            lines.push('        }}' + last);
        });
        lines.push('    ]', '}');
        return lines.join('\n') + '\n';
    }

    // what is wrong with a layout, in words: [{level, text}]; errors: pad_blob.py refuses it, DGX-DI does not open it
    function problems(layout, d) {
        const out = [], error = text => out.push({ level: 'error', text, item: out.item }), warning = text => out.push({ level: 'warning', text, item: out.item });
        const items = layout.items || [];
        if (items.length < 1 || items.length > Limits.items) error(`a layout has 1 to ${Limits.items} items, this one ${items.length}`);
        const pls = d ? places(layout, d) : [];
        items.forEach((it, k) => {
            const at = `item ${k + 1} (${it.kind}${it.out ? ' ' + it.out : ''})`;
            out.item = k; /* each told while this item is looked at belongs to it */
            if (!Kinds.includes(it.kind)) error(`${at}: no kind ${it.kind}`);
            if (!(it.r >= 1)) error(`${at}: a radius of a tenth of a millimetre at least`);
            if (it.kind === 'button' && !Bits.includes(it.out)) error(`${at}: a button's out is the name of a bit`);
            if (it.kind === 'stick' && it.out && !['left', 'right'].includes(it.out)) error(`${at}: a stick's out is left or right`);
            for (const [name, shapes] of Object.entries(it.pictures || {})) {
                if (!(Pictures[it.kind] || []).includes(name)) error(`${at}: a ${it.kind} has no picture ${name}`);
                (shapes || []).forEach((sh, i) => {
                    if (!Shapes.includes(sh.shape)) error(`${at}, ${name}, shape ${i + 1}: no shape ${sh.shape}`);
                    if (sh.shape === 'polygon' && !(sh.points && sh.points.length >= 3)) error(`${at}, ${name}, shape ${i + 1}: a polygon of 3 points at least`);
                    if (sh.shape === 'line' && !(sh.points && sh.points.length >= 2)) error(`${at}, ${name}, shape ${i + 1}: a line of 2 points at least`);
                });
            }
            if (d) {
                const p = pls[k];
                if (p.x - p.r < 0 || p.y - p.r < 0 || p.x + p.r >= d.width || p.y + p.r >= d.height) warning(`${at}: past the edge of the display`);
                if (p.r * 2 * d.widthMm / d.width < 8) warning(`${at}: under 8 mm across, fingers miss it`);
                pls.forEach((q, j) => {
                    if (j > k && (p.x - q.x) ** 2 + (p.y - q.y) ** 2 < (p.r + q.r) ** 2) warning(`${at} and item ${j + 1}: they overlap (the first has the finger)`);
                });
            }
        });
        delete out.item;
        return out;
    }

    function check(json) {
        if (!json || json.format !== Format.name) return 'not a layout of the pad of DGX-DI';
        if (json.version > Format.version) return `a layout of version ${json.version}: this editor reads up to ${Format.version}`;
        if (!Array.isArray(json.items)) return 'a layout without its items';
        return null;
    }

    // the classic layout: dgx_di_pad_classic of DGX-DI, item for item (its colors those its RGB565 has)
    function classic() {
        return {
            format: Format.name, version: Format.version, name: 'classic',
            items: [
                { kind: 'cross', fromX: 'start', fromY: 'end', x: 300, y: 300, r: 200 },
                { kind: 'button', fromX: 'end', fromY: 'end', x: 300, y: 220, r: 70, out: 'A', color: hex565(0x2e65) },
                { kind: 'button', fromX: 'end', fromY: 'end', x: 180, y: 340, r: 70, out: 'B', color: hex565(0xd8c3) },
                { kind: 'button', fromX: 'end', fromY: 'end', x: 420, y: 340, r: 70, out: 'X', color: hex565(0x249d) },
                { kind: 'button', fromX: 'end', fromY: 'end', x: 300, y: 460, r: 70, out: 'Y', color: hex565(0xee24) },
                { kind: 'button', fromX: 'middle', fromY: 'end', x: -150, y: 120, r: 50, out: 'BACK' },
                { kind: 'button', fromX: 'middle', fromY: 'end', x: 150, y: 120, r: 50, out: 'START' }
            ]
        };
    }

    // --- layouts to begin with: DGX-DI's own (as examples/ has them), then the pads of the usual games, in the default
    // look; in tenths of a millimetre for the 7" and larger, the mini one for a display of 2.8"

    const GREEN = hex565(0x2e65), RED = hex565(0xd8c3), BLUE = hex565(0x249d), YELLOW = hex565(0xee24), STICK = '#4a5a78';
    const button = (fromX, fromY, x, y, r, out, color) => ({ kind: 'button', fromX, fromY, x, y, r, out, ...(color ? { color } : {}) });
    const cross = (x, y, r) => ({ kind: 'cross', fromX: 'start', fromY: 'end', x, y, r });
    const backStart = () => [button('middle', 'end', -150, 120, 50, 'BACK'), button('middle', 'end', 150, 120, 50, 'START')];
    const shoulders = (left, right) => [button('start', 'start', 220, 150, 90, left), button('end', 'start', 220, 150, 90, right)];
    const pad = (name, items) => ({ format: Format.name, version: Format.version, name, items });
    const Templates = [
        { name: 'classic', own: true, label: 'Classic', about: 'a cross, A B X Y, BACK START: drawn by default',
            tip: 'Крестовина, A B X Y, BACK и START: её DGX-DI рисует сам, без блоба (встроена)', make: classic },
        { name: 'flat', own: true, label: 'Flat', about: 'the classic one of plain shapes',
            tip: 'Классическая, нарисованная простыми фигурами (встроена)', make: () => ({ ...themed(classic(), 'Flat'), name: 'flat' }) },
        { name: 'twin', own: true, label: 'Twin', about: 'a floating stick, A B X Y, LB RB, BACK START, of glass',
            tip: 'Плавающий стик, A B X Y, плечевые LB и RB, BACK и START; стеклянная (встроена)',
            make: () => themed(pad('twin', [{ kind: 'stick', fromX: 'start', fromY: 'end', x: 280, y: 280, r: 190, out: 'left', floating: true, color: STICK },
                button('end', 'end', 260, 200, 75, 'A', GREEN), button('end', 'end', 150, 310, 75, 'B', RED), button('end', 'end', 370, 310, 75, 'X', BLUE),
                button('end', 'end', 260, 420, 75, 'Y', YELLOW), ...shoulders('LB', 'RB'), ...backStart()]), 'Glass') },
        { name: 'retro', label: 'Retro', about: 'a cross, B and A: NES, Game Boy',
            tip: 'Крестовина, B и A наискосок, BACK (SELECT) и START: как у NES и Game Boy',
            make: () => pad('retro', [cross(300, 300, 200), button('end', 'end', 430, 230, 85, 'B', RED), button('end', 'end', 200, 330, 85, 'A', RED), ...backStart()]) },
        { name: 'super', label: 'Super', about: 'a cross, A B X Y, LB RB: SNES',
            tip: 'Классическая и плечевые LB и RB в верхних углах: как у SNES',
            make: () => pad('super', [...classic().items, ...shoulders('LB', 'RB')]) },
        { name: 'arcade', label: 'Arcade', about: 'a cross, six buttons in two rows: fighting',
            tip: 'Крестовина на восемь сторон и шесть кнопок в два ряда, X Y RB над A B RT: для драк',
            make: () => pad('arcade', [cross(300, 300, 200),
                button('end', 'end', 460, 390, 70, 'X', BLUE), button('end', 'end', 300, 420, 70, 'Y', YELLOW), button('end', 'end', 140, 410, 70, 'RB'),
                button('end', 'end', 460, 220, 70, 'A', GREEN), button('end', 'end', 300, 250, 70, 'B', RED), button('end', 'end', 140, 240, 70, 'RT'), ...backStart()]) },
        { name: 'shooter', label: 'Shooter', about: 'two sticks, to go and to aim; LT RT',
            tip: 'Два стика: левый (плавающий) — идти, правый — целиться; LT и RT в верхних углах, BACK и START',
            make: () => pad('shooter', [{ kind: 'stick', fromX: 'start', fromY: 'end', x: 280, y: 280, r: 190, out: 'left', floating: true, color: STICK },
                { kind: 'stick', fromX: 'end', fromY: 'end', x: 280, y: 280, r: 190, out: 'right', color: STICK }, ...shoulders('LT', 'RT'), ...backStart()]) },
        { name: 'platformer', label: 'Platformer', about: 'left, right, a jump (A) and a shot (B)',
            tip: 'Кнопки «влево» и «вправо» под левым пальцем; прыжок (A) и выстрел (B) под правым; BACK и START',
            make: () => pad('platformer', [button('start', 'end', 170, 200, 110, 'LEFT'), button('start', 'end', 420, 200, 110, 'RIGHT'),
                button('end', 'end', 190, 220, 120, 'A', GREEN), button('end', 'end', 430, 180, 90, 'B', RED), ...backStart()]) },
        { name: 'racing', label: 'Racing', about: 'left, right to steer; LT to brake, RT to go',
            tip: 'Руль кнопками «влево» и «вправо»; тормоз (LT) и газ (RT, побольше) справа; START',
            make: () => pad('racing', [button('start', 'end', 175, 215, 120, 'LEFT'), button('start', 'end', 445, 215, 120, 'RIGHT'),
                button('end', 'end', 450, 180, 105, 'LT', RED), button('end', 'end', 180, 260, 140, 'RT', GREEN), button('middle', 'end', 0, 120, 50, 'START')]) },
        { name: 'mini', label: 'Mini', about: 'a cross, A B: for a display of 2.8" (CYD)',
            tip: 'Для маленького экрана (CYD, 2.8″): крестовина, A и B, BACK и START в верхних углах. Резистивная панель берёт один палец: одно нажатие за раз',
            make: () => pad('mini', [{ kind: 'cross', fromX: 'start', fromY: 'end', x: 150, y: 150, r: 130 },
                button('end', 'end', 100, 190, 65, 'A', GREEN), button('end', 'end', 220, 100, 65, 'B', RED),
                button('start', 'start', 60, 60, 45, 'BACK'), button('end', 'start', 60, 60, 45, 'START')]) }
    ];
    // a template by its name, made anew (the first, the classic one, if there is none of the name)
    const template = name => (Templates.find(t => t.name === name) || Templates[0]).make();

    // one picture's shapes, its middle at cx, cy, in the pixels of a display (or of a view as large as one likes)
    const drawPicture = (ctx, it, shapes, cx, cy, d) => picture(ctx, it, shapes, cx, cy, scale(d));

    // --- the blob dgx_di_pad_open() takes, as tools/pad_blob.py makes it (byte for byte: test.js compares them); a
    // layout DGX-DI would not open throws what is wrong with it

    const FROM = { start: 0, middle: 1, end: 2 }, PAINT = { item: 1, light: 2, dark: 3 }, SLOTS = 9, NONE = 0xffff;
    class Bytes {
        constructor() { this.parts = [], this.length = 0; }
        push(format, ...values) { // a struct format of B (u8), H (u16), h (i16), I (u32), little-endian
            const sizes = { B: 1, H: 2, h: 2, I: 4 }, size = [...format].reduce((n, f) => n + sizes[f], 0);
            const buf = new DataView(new ArrayBuffer(size));
            let at = 0;
            [...format].forEach((f, i) => {
                const v = values[i];
                if (f === 'B') buf.setUint8(at, v);
                else if (f === 'H') buf.setUint16(at, v, true);
                else if (f === 'h') buf.setInt16(at, v, true);
                else buf.setUint32(at, v >>> 0, true);
                at += sizes[f];
            });
            this.parts.push(new Uint8Array(buf.buffer)), this.length += size;
        }
        bytes(raw) { this.parts.push(raw), this.length += raw.length; }
    }
    const whole = (v, what, least = -32768) => {
        if (!Number.isInteger(v) || v < least || v > 32767) throw new Error(`${what}: a whole number of tenths of a millimetre from ${least}, not ${JSON.stringify(v)}`);
        return v;
    };
    const color565 = (c, what) => {
        if (typeof c !== 'string' || !/^#[0-9a-fA-F]{6}$/.test(c)) throw new Error(`${what}: a color is #rrggbb, not ${JSON.stringify(c)}`);
        return rgb565(c);
    };
    function toBlob(layout) {
        if (!layout || layout.format !== Format.name) throw new Error(`not a layout of the pad of DGX-DI (format ${JSON.stringify(Format.name)})`);
        if (!Number.isInteger(layout.version) || layout.version > Format.version) throw new Error(`a layout of version ${JSON.stringify(layout.version)}: this tool reads up to ${Format.version}`);
        const items = layout.items;
        if (!Array.isArray(items) || items.length < 1 || items.length > Limits.items) throw new Error(`a layout has 1 to ${Limits.items} items`);
        const itemData = new Bytes(), shapeData = new Bytes(), pointData = new Bytes();
        let shapes = 0, points = 0;
        items.forEach((it, k) => {
            const where = `item ${k + 1}`, kind = it.kind;
            if (!Kinds.includes(kind)) throw new Error(`${where}: a kind of button, cross or stick, not ${JSON.stringify(kind)}`);
            const fx = it.fromX === undefined ? 'start' : it.fromX, fy = it.fromY === undefined ? 'start' : it.fromY;
            if (!(fx in FROM) || !(fy in FROM)) throw new Error(`${where}: fromX and fromY are start, middle or end`);
            const x = whole(it.x === undefined ? 0 : it.x, where + ', x'), y = whole(it.y === undefined ? 0 : it.y, where + ', y'), r = whole(it.r, where + ', r', 1);
            let out = 0;
            if (kind === 'button') {
                if (!Bits.includes(it.out)) throw new Error(`${where}: a button's out is one of ${Bits.join(' ')}, not ${JSON.stringify(it.out)}`);
                out = bit(it.out);
            } else if (kind === 'stick') {
                const o = it.out === undefined ? 'left' : it.out;
                if (o !== 'left' && o !== 'right') throw new Error(`${where}: a stick's out is left or right`);
                out = o === 'right' ? 1 : 0;
            }
            const ways = (it.four ? 1 : 0) | (it.floating ? 2 : 0), color = it.color ? color565(it.color, where) : 0;
            const pictures = it.pictures === undefined ? null : it.pictures;
            if (pictures !== null && (typeof pictures !== 'object' || Array.isArray(pictures) || Object.keys(pictures).some(n => !Pictures[kind].includes(n))))
                throw new Error(`${where}: a ${kind}'s pictures are ${Pictures[kind].join(', ')}`);
            const slots = [];
            for (const name of [...Pictures[kind], ...Array(SLOTS - Pictures[kind].length).fill(null)]) {
                const of = name && pictures && pictures[name] !== undefined ? pictures[name] : [];
                if (!Array.isArray(of)) throw new Error(`${where}, ${name}: a picture is a list of shapes`);
                slots.push([shapes, of.length]);
                of.forEach((sh, i) => {
                    const w = `${where}, ${name}, shape ${i + 1}`, kindOf = sh.shape;
                    if (!Shapes.includes(kindOf)) throw new Error(`${w}: a shape of ${Shapes.join(', ')}, not ${JSON.stringify(kindOf)}`);
                    const c = sh.color === undefined ? 'item' : sh.color, [paint, own] = c in PAINT ? [PAINT[c], 0] : [0, color565(c, w)];
                    const pts = (kindOf === 'polygon' || kindOf === 'line') && sh.points !== undefined ? sh.points : [];
                    if (kindOf === 'polygon' && pts.length < 3) throw new Error(`${w}: a polygon of 3 points at least`);
                    if (kindOf === 'line' && pts.length < 2) throw new Error(`${w}: a line of 2 points at least`);
                    const sx = whole(sh.x === undefined ? 0 : sh.x, w + ', x'), sy = whole(sh.y === undefined ? 0 : sh.y, w + ', y');
                    const sw = whole(kindOf === 'ring' ? (sh.width === undefined ? 1 : sh.width) : (sh.w === undefined ? 0 : sh.w), w + ', w', 0);
                    const shh = whole(sh.h === undefined ? 0 : sh.h, w + ', h', 0);
                    const sr = whole(kindOf === 'line' ? (sh.width === undefined ? 1 : sh.width) : (sh.r === undefined ? 0 : sh.r), w + ', r', 0);
                    shapeData.push('BBHhhhhhHHH', SHAPES_OF[kindOf], paint, own, sx, sy, sw, shh, sr, points, pts.length, 0);
                    pts.forEach((p, j) => {
                        if (!(Array.isArray(p) && p.length === 2)) throw new Error(`${w}, point ${j + 1}: [x, y]`);
                        pointData.push('hh', whole(p[0], w + ", a point's x"), whole(p[1], w + ", a point's y"));
                    });
                    points += pts.length;
                    shapes += 1;
                    if (shapes > 0xffff || points > 0xffff) throw new Error('more than 65535 shapes or points');
                });
            }
            itemData.push('BBBBhhhHI', Kinds.indexOf(kind), FROM[fx], FROM[fy], ways, x, y, r, color, out);
            for (const [first, n] of slots) itemData.push('HH', n ? first : 0, n);
            itemData.push('BBH', pictures !== null ? 1 : 0, 0, 0);
        });
        const name = layout.name || '', strings = name ? new TextEncoder().encode(name + '\0') : new Uint8Array(0);
        const header = new Bytes();
        header.bytes(new TextEncoder().encode('DGXP'));
        header.push('BBHHHHH', Format.version, items.length, shapes, points, name ? 0 : NONE, 0, 0);
        const out = new Uint8Array(header.length + itemData.length + shapeData.length + pointData.length + strings.length);
        let at = 0;
        for (const part of [...header.parts, ...itemData.parts, ...shapeData.parts, ...pointData.parts, strings]) out.set(part, at), at += part.length;
        return out;
    }
    const SHAPES_OF = { circle: 0, ring: 1, rect: 2, polygon: 3, line: 4 };

    // a C name of a layout's name, and the blob as a C file (as pad_blob.py --c writes it): const uint8_t NAME[], NAME_size
    function ident(name) {
        let s = (name || 'pad').toLowerCase().replace(/[^a-z0-9]+/g, '_').replace(/^_+|_+$/g, '') || 'pad';
        return /^[0-9]/.test(s) ? '_' + s : s;
    }
    function toC(name, data, source) {
        const lines = [`/* The pad layout of ${source}, made by the pad editor of DGX-DI: dgx_di_pad_open(..., ${name}, ${name}_size) */`,
            '#include <stddef.h>', '#include <stdint.h>', '', `/* ${source}: ${data.length} bytes */`, `const uint8_t ${name}[] = {`];
        for (let i = 0; i < data.length; i += 16) lines.push('    ' + [...data.slice(i, i + 16)].map(b => '0x' + b.toString(16).padStart(2, '0')).join(', ') + ',');
        lines.push('};', `const size_t ${name}_size = sizeof ${name};`, '');
        return lines.join('\n');
    }

    return { Format, Kinds, Bits, BitGroups, Palette, nameOf, Pictures, Shapes, Paints, Limits, Displays, bit, pixels, scale, places, unplace, rgb565, hex565, lighter, darker, itemColor, shapeColor, weightOf, mix, drawItem, drawPicture, itemAt, ways, hold, Themes, themed, scaled, toJson, problems, check, classic, Templates, template, toBlob, ident, toC };
})();

if (typeof module === 'object') module.exports = PadLayout;
