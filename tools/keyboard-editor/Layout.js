// The model of a layout of DGX-DI's keyboard: what the editor edits and shows. Its file is JSON (toJson; the format is
// told in tools/keyboard_blob.py, which makes the blob DGX-DI opens of it).
//
// The geometry is integer, step for step as src/dgx_di_keyboard.c has it (place, lay): the canvas shows the keys where
// DGX-DI puts them. A change here is a change there; test.js compares the two.
const KeyboardLayout = (() => {
    const Format = { name: 'dgx-di-keyboard', version: 1 };
    const SHIFT = 0x110000, PAGE = 0x110100;
    const Actions = { erase: 0x08, done: 0x0a, cancel: 0x1b };
    const Limits = { pages: 4, rows: 5, keys: 48 };

    // the displays DGX-DI was tried on, as they stand in the programs that run on them
    const Displays = [
        { name: '7" Guition JC1060P470', width: 1024, height: 600, widthMm: 154, heightMm: 86 },
        { name: '10.1" Guition JC8012P4A1C', width: 1280, height: 800, widthMm: 217, heightMm: 136 },
        { name: 'CYD (ESP32-2432S028R)', width: 320, height: 240, widthMm: 58, heightMm: 43 }
    ];

    // tenths of a millimetre in pixels along an axis of px pixels over mm millimetres, rounded
    const pixels = (tenths, px, mm) => Math.floor((tenths * px * 2 + 10 * mm) / (20 * mm));
    const quarters = key => key.width || 4;
    const code = key => key.action === 'shift' ? SHIFT : key.action ? Actions[key.action] : key.page !== undefined ? PAGE + key.page : key.char.codePointAt(0);
    const special = c => c < 0x20 || c >= SHIFT;
    const rowQuarters = row => row.reduce((q, key) => q + quarters(key), 0);

    // the band of the keyboard on a display: as place() of the C, null where it makes none
    function place(layout, display, field) {
        const W = display.width, H = display.height, wmm = display.widthMm, hmm = display.heightMm;
        const gapX = pixels(layout.gap, W, wmm), gapY = pixels(layout.gap, H, hmm);
        const marginX = pixels(layout.margin, W, wmm), marginY = pixels(layout.margin, H, hmm);
        const radius = Math.floor((pixels(layout.radius, W, wmm) + pixels(layout.radius, H, hmm) + 1) / 2);
        let unit = pixels(layout.unitW, W, wmm), inner = 0, rows = 0;
        for (const page of layout.pages) {
            for (const row of page.rows) {
                const room = W - 2 * marginX - (row.length - 1) * gapX;
                const fit = room > 0 ? Math.floor(room * 4 / rowQuarters(row)) : 0;
                if (fit < unit) unit = fit;
            }
            if (page.rows.length > rows) rows = page.rows.length;
        }
        if (unit < 1) return null;
        for (const page of layout.pages)
            for (const row of page.rows) inner = Math.max(inner, Math.floor(rowQuarters(row) * unit / 4) + (row.length - 1) * gapX);
        let keyH = pixels(layout.keyH, H, hmm);
        if (rows * keyH + (rows - 1) * gapY + 2 * marginY > H) keyH = Math.trunc((H - 2 * marginY - (rows - 1) * gapY) / rows);
        if (keyH < 1) return null;
        const w = inner + 2 * marginX, h = rows * keyH + (rows - 1) * gapY + 2 * marginY, x = Math.floor((W - w) / 2);
        let y = H - h;
        // the field under it: at the top instead
        if (field && field.w > 0 && field.h > 0 && field.x < x + w && x < field.x + field.w && field.y < y + h && y < field.y + field.h) y = 0;
        return { x, y, w, h, unit, gapX, gapY, keyH, marginX, marginY, radius, rows };
    }

    // the keys of a page where they are, as lay() of the C: [{x, y, w, h, row, index, key}]
    function lay(layout, band, pageIndex) {
        const page = layout.pages[pageIndex], inner = band.w - 2 * band.marginX, step = band.keyH + band.gapY;
        const top = band.y + band.marginY + Math.floor((band.rows - page.rows.length) * step / 2);
        const keys = [];
        page.rows.forEach((row, r) => {
            const width = Math.floor(rowQuarters(row) * band.unit / 4) + (row.length - 1) * band.gapX;
            const x0 = band.x + band.marginX + Math.floor((inner - width) / 2);
            let q = 0;
            row.forEach((key, i) => {
                const left = x0 + Math.floor(q * band.unit / 4) + i * band.gapX;
                q += quarters(key);
                const right = x0 + Math.floor(q * band.unit / 4) + i * band.gapX;
                keys.push({ x: left, y: top + r * step, w: right - left, h: band.keyH, row: r, index: i, key });
            });
        });
        return keys;
    }

    // what a key types with shift as it is (a code point), or does
    function typed(key, shift) {
        if (key.char !== undefined) return shift && key.upper ? key.upper.codePointAt(0) : key.char.codePointAt(0);
        return code(key);
    }

    // the text drawn on a key; null: its sign (erase, done, cancel, shift) or nothing (a space bar)
    function label(key, shift) {
        if (key.label) return key.label;
        const c = code(key);
        if (special(c) || c === 0x20) return null;
        return String.fromCodePoint(typed(key, shift));
    }

    // RGB565 as DGX makes it of #rrggbb (dgx_rgb_to_16: the low bits dropped), and back for the canvas
    function rgb565(hex) {
        const v = parseInt(hex.slice(1), 16), r = v >> 16 & 255, g = v >> 8 & 255, b = v & 255;
        return (r & 0xf8) << 8 | (g & 0xfc) << 3 | (b & 0xf8) >> 3;
    }
    function hex565(c) {
        const r = c >> 11 & 31, g = c >> 5 & 63, b = c & 31;
        const v = (r << 3 | r >> 2) << 16 | (g << 2 | g >> 4) << 8 | (b << 3 | b >> 2);
        return '#' + v.toString(16).padStart(6, '0');
    }
    // a to b, t of n of the way, each of the three colors: mix() of the C
    function mix565(a, b, t, n) {
        const ar = a >> 11, ag = a >> 5 & 63, ab = a & 31, br = b >> 11, bg = b >> 5 & 63, bb = b & 31;
        const part = (x, y) => x + Math.trunc((y - x) * t / n);
        return part(ar, br) << 11 | part(ag, bg) << 5 | part(ab, bb);
    }

    // what is wrong with a layout, in words: [{level: 'error' | 'warning', text}]; errors: DGX-DI does not open it
    function problems(layout, display) {
        const out = [];
        const error = text => out.push({ level: 'error', text }), warning = text => out.push({ level: 'warning', text });
        const pages = layout.pages || [];
        if (pages.length < 1 || pages.length > Limits.pages) error(`a layout has 1 to ${Limits.pages} pages, this one ${pages.length}`);
        for (const [name, v] of [['key height', layout.keyH], ['key width', layout.unitW]])
            if (!(v > 0)) error(`the ${name} is not above 0`);
        for (const [name, v] of [['gap', layout.gap], ['margin', layout.margin], ['radius', layout.radius]])
            if (!(v >= 0)) error(`the ${name} is below 0`);
        pages.forEach((page, p) => {
            const where = `page ${p + 1} (${page.name || 'no name'})`;
            const n = page.rows.reduce((n, row) => n + row.length, 0);
            if (page.rows.length < 1 || page.rows.length > Limits.rows) error(`${where}: 1 to ${Limits.rows} rows, here ${page.rows.length}`);
            if (n > Limits.keys) error(`${where}: at most ${Limits.keys} keys, here ${n}`);
            if (page.rows.length !== pages[0].rows.length) warning(`${where}: ${page.rows.length} rows, the first page ${pages[0].rows.length}: the band is as high as the most`);
            page.rows.forEach((row, r) => {
                if (!row.length) error(`${where}, row ${r + 1}: no keys`);
                row.forEach((key, i) => {
                    const at = `${where}, row ${r + 1}, key ${i + 1}`;
                    if (key.char !== undefined && [...key.char].length !== 1) error(`${at}: a character key types one character`);
                    if (key.upper !== undefined && key.upper !== '' && [...key.upper].length !== 1) error(`${at}: with shift, one character`);
                    if (key.page !== undefined && !(key.page >= 0 && key.page < pages.length)) error(`${at}: no page ${key.page + 1}`);
                    if (key.page === p) warning(`${at}: goes to its own page`);
                    if (key.page !== undefined && !key.label) warning(`${at}: a key of a page shows its label only, and has none`);
                    if (key.width !== undefined && !(key.width >= 1 && key.width <= 255)) error(`${at}: a width of 1 to 255 quarters`);
                });
            });
        });
        const font = layout.font;
        if (font && !(typeof font.dgx === 'string' && font.dgx) && !((font.postscript || font.family) && font.px >= 1))
            warning('the font of the labels: neither a font of DGX by its name nor a face of a file at a size in pixels');
        if (display && !out.some(o => o.level === 'error') && !place(layout, display)) error(`it does not fit the display of ${display.width}x${display.height}`);
        return out;
    }

    // a key with its fields in the order of the format: what it types or does, then how it looks
    function cleanKey(key) {
        const out = {};
        for (const name of ['char', 'upper', 'action', 'page', 'label', 'width'])
            if (key[name] !== undefined && key[name] !== '' && !(name === 'width' && key[name] === 4)) out[name] = key[name];
        return out;
    }
    const inline = value => '{' + Object.entries(value).map(([n, v]) => JSON.stringify(n) + ': ' + JSON.stringify(v)).join(', ') + '}';
    // the font the labels are made for, its fields in the order of the format: a font of DGX by its name, or a face of a
    // font file (as fontconfig names it) at a size in pixels of the display
    function cleanFont(font) {
        const out = {};
        for (const name of ['dgx', 'family', 'style', 'postscript', 'px']) if (font[name] !== undefined && font[name] !== '') out[name] = font[name];
        return out;
    }

    // the layout as the file of the editor: one row of keys a line, so that it reads and compares well
    function toJson(layout) {
        const t = layout.theme, lines = ['{'];
        lines.push(`    "format": ${JSON.stringify(Format.name)},`, `    "version": ${Format.version},`, `    "name": ${JSON.stringify(layout.name || '')},`);
        for (const name of ['keyH', 'unitW', 'gap', 'margin', 'radius']) lines.push(`    ${JSON.stringify(name)}: ${layout[name]},`);
        lines.push('    "theme": ' + inline({ backTop: t.backTop, backBottom: t.backBottom, key: t.key, keySpecial: t.keySpecial, keyPressed: t.keyPressed, label: t.label }) + ',');
        if (layout.font) lines.push('    "font": ' + inline(cleanFont(layout.font)) + ',');
        lines.push('    "pages": [');
        layout.pages.forEach((page, p) => {
            lines.push(`        {"name": ${JSON.stringify(page.name || '')}, "rows": [`);
            page.rows.forEach((row, r) => lines.push('            [' + row.map(k => inline(cleanKey(k))).join(', ') + ']' + (r < page.rows.length - 1 ? ',' : '')));
            lines.push('        ]}' + (p < layout.pages.length - 1 ? ',' : ''));
        });
        lines.push('    ]', '}');
        return lines.join('\n') + '\n';
    }

    // a new layout to begin with: a code of digits, in the colors of DGX-DI's own
    function starter() {
        const digit = c => ({ char: c });
        return {
            format: Format.name, version: Format.version, name: 'new', keyH: 100, unitW: 200, gap: 15, margin: 20, radius: 20,
            theme: { backTop: '#2b2f36', backBottom: '#1b1e23', key: '#4b525c', keySpecial: '#353a42', keyPressed: '#8a94a3', label: '#ffffff' },
            pages: [{ name: 'digits', rows: [[...'123'].map(digit), [...'456'].map(digit), [...'789'].map(digit), [{ action: 'erase' }, digit('0'), { action: 'done' }]] }]
        };
    }

    // --- layouts to begin with: DGX-DI's own (as examples/ has them), then others, in the colors of DGX-DI's own

    const THEME = { backTop: '#2b2f36', backBottom: '#1b1e23', key: '#4b525c', keySpecial: '#353a42', keyPressed: '#8a94a3', label: '#ffffff' };
    const KEYS = { keyH: 100, unitW: 200, gap: 15, margin: 20, radius: 20 }, TYPING = { keyH: 100, unitW: 160, gap: 12, margin: 15, radius: 15 };
    const chars = s => [...s].map(c => ({ char: c })), letters = s => [...s].map(c => ({ char: c, upper: c.toUpperCase() }));
    const board = (name, sizes, pages) => ({ format: Format.name, version: Format.version, name, ...sizes, theme: { ...THEME }, pages });
    const erase = width => ({ action: 'erase', ...(width ? { width } : {}) }), done = width => ({ action: 'done', ...(width ? { width } : {}) });
    const toPage = (page, label, width) => ({ page, label, ...(width ? { width } : {}) });
    const qwerty = () => [letters('qwertyuiop'), letters('asdfghjkl'), [{ action: 'shift', width: 6 }, ...letters('zxcvbnm'), erase(6)]];
    const Templates = [
        { name: 'digits', own: true, label: 'Digits', about: 'a code of digits', tip: 'Код из цифр: 1–9, 0, стереть, готово (встроена)',
            make: () => board('digits', KEYS, [{ name: 'digits', rows: [chars('123'), chars('456'), chars('789'), [erase(), { char: '0' }, done()]] }]) },
        { name: 'latin', own: true, label: 'Latin', about: 'QWERTY; digits and signs', tip: 'Латиница QWERTY с заглавными; страница цифр и знаков (встроена)',
            make: () => board('latin', TYPING, [
                { name: 'letters', rows: [...qwerty(), [toPage(1, '123', 6), { action: 'cancel' }, { char: ' ', width: 20 }, { char: '.' }, done(6)]] },
                { name: 'digits and signs', rows: [chars('1234567890'), chars('-/:;()&@"\''), [...chars('_#+=*%?!'), erase(6)],
                    [toPage(0, 'abc', 6), { action: 'cancel' }, { char: ' ', width: 20 }, { char: ',' }, done(6)]] }]) },
        { name: 'phone', label: 'Phone', about: 'a number to call: * #, +', tip: 'Телефонный номер: 1–9, * 0 #, плюс, стереть, готово',
            make: () => board('phone', KEYS, [{ name: 'phone', rows: [chars('123'), chars('456'), chars('789'), chars('*0#'), [{ char: '+' }, erase(), done()]] }]) },
        { name: 'number', label: 'Number', about: 'a number with a sign and a point', tip: 'Число: цифры, минус, точка, стереть; отмена и готово',
            make: () => board('number', KEYS, [{ name: 'number', rows: [[...chars('123'), erase()], chars('456-'), chars('789.'), [{ action: 'cancel' }, { char: '0', width: 8 }, done()]] }]) },
        { name: 'hex', label: 'Hex', about: '0–9 A–F', tip: 'Шестнадцатеричное: 0–9 и A–F, стереть, готово',
            make: () => board('hex', KEYS, [{ name: 'hex', rows: [chars('123AB'), chars('456CD'), chars('789EF'), [erase(8), { char: '0' }, done(8)]] }]) },
        { name: 'russian', label: 'Russian', about: 'ЙЦУКЕН; digits and signs; Latin',
            tip: 'Русская ЙЦУКЕН с заглавными; страницы цифр и знаков и латиницы. Шрифт программы должен знать кириллицу (DGX font2c из TTF)',
            make: () => board('russian', { ...TYPING, unitW: 140 }, [
                { name: 'русские', rows: [letters('йцукенгшщзхъ'), letters('фывапролджэ'), [{ action: 'shift', width: 6 }, ...letters('ячсмитьбюё'), erase(6)],
                    [toPage(1, '123', 6), toPage(2, 'abc'), { action: 'cancel' }, { char: ' ', width: 16 }, { char: ',' }, { char: '.' }, done(6)]] },
                { name: 'цифры и знаки', rows: [chars('1234567890'), chars('-/:;()&@"\''), [...chars('_#№+=*%?!'), erase(6)],
                    [toPage(0, 'абв', 6), toPage(2, 'abc'), { action: 'cancel' }, { char: ' ', width: 16 }, { char: ',' }, done(6)]] },
                { name: 'латиница', rows: [...qwerty(), [toPage(1, '123', 6), toPage(0, 'абв'), { action: 'cancel' }, { char: ' ', width: 16 }, { char: '.' }, done(6)]] }]) },
        { name: 'german', label: 'German', about: 'QWERTZ with ä ö ü ß; digits and signs',
            tip: 'Немецкая QWERTZ с заглавными: ü после p, ö и ä после l, ß после m; страница цифр и знаков с €. Шрифт программы должен знать эти буквы',
            make: () => board('german', { ...TYPING, unitW: 140 }, [
                { name: 'Buchstaben', rows: [letters('qwertzuiopü'), letters('asdfghjklöä'), [{ action: 'shift', width: 6 }, ...letters('yxcvbnm'), { char: 'ß' }, erase(6)],
                    [toPage(1, '123', 6), { action: 'cancel' }, { char: ' ', width: 20 }, { char: '.' }, done(6)]] },
                { name: 'Ziffern und Zeichen', rows: [chars('1234567890'), chars('-/:;()€&@"'), [...chars('_#+=*%?!\''), erase(6)],
                    [toPage(0, 'abc', 6), { action: 'cancel' }, { char: ' ', width: 20 }, { char: ',' }, done(6)]] }]) }
    ];
    // a template by its name, made anew (the first if there is none of the name)
    const template = name => (Templates.find(t => t.name === name) || Templates[0]).make();

    // a project file of the editor: of this format, or an error in words
    function check(json) {
        if (!json || json.format !== Format.name) return 'not a layout of the keyboard of DGX-DI';
        if (json.version > Format.version) return `a layout of version ${json.version}: this editor reads up to ${Format.version}`;
        if (!Array.isArray(json.pages) || !json.theme) return 'a layout without its pages or theme';
        return null;
    }

    return { Format, SHIFT, PAGE, Actions, Limits, Displays, pixels, quarters, code, special, place, lay, typed, label, rgb565, hex565, mix565, problems, cleanKey, toJson, starter, check, Templates, template };
})();

if (typeof module === 'object') module.exports = KeyboardLayout;
