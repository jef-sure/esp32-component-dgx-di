// The state of the editor: the layout edited (KeyboardLayout's), how it is seen, the trial of typing; undo and redo;
// kept in the browser between visits.
const StorageKey = 'dgx-di-keyboard-editor';
// the C of the font of DGX the labels were last drawn in, kept apart (it may not fit; the layout is kept anyway)
const FontStorageKey = StorageKey + '-font';
// the browser's own font of the labels, as close to the program's DGX font as the browser has
const BrowserFont = '"DejaVu Sans", Verdana, sans-serif';
// what a font of the labels is known by: a font of DGX by its name, a face of a file by its PostScript name
const fontIdentity = font => !font ? '' : font.dgx ? 'dgx:' + font.dgx : 'face:' + (font.postscript || `${font.family} ${font.style}`);
// the fonts loaded in the page, by what they are known by: {kind ('system', 'file', 'dgx'), css (a family of the
// page) or dgx (its glyphs), has (whether it has a code point; null: not told)}; not kept by Vue, its count is
const LoadedFonts = new Map();
let fontAliases = 0;

// themes to begin a look of one's own with: the band from top to bottom, the keys, the labels
const ThemePresets = {
    Slate: { backTop: '#2b2f36', backBottom: '#1b1e23', key: '#4b525c', keySpecial: '#353a42', keyPressed: '#8a94a3', label: '#ffffff' },
    Light: { backTop: '#d6d9de', backBottom: '#c3c7ce', key: '#fdfdfd', keySpecial: '#a9afb9', keyPressed: '#7d8796', label: '#101214' },
    Night: { backTop: '#05070c', backBottom: '#0b1020', key: '#1a2238', keySpecial: '#11182a', keyPressed: '#3d5a99', label: '#c8d6ff' },
    Ocean: { backTop: '#0d3b66', backBottom: '#072440', key: '#1d5c94', keySpecial: '#134877', keyPressed: '#5fa8e8', label: '#ffffff' },
    Forest: { backTop: '#1e3d2b', backBottom: '#0f2318', key: '#33603f', keySpecial: '#244a30', keyPressed: '#7fbf8e', label: '#f2fff4' },
    Amber: { backTop: '#2a1a05', backBottom: '#140c02', key: '#5a3a0e', keySpecial: '#3d270a', keyPressed: '#d9902a', label: '#ffd38a' }
};

const useKeyboardStore = Pinia.defineStore('keyboard', {
    state() {
        return {
            layout: KeyboardLayout.starter(),
            edit: {
                display: { ...KeyboardLayout.Displays[0] },
                field: 'top',   // the program's field in the picture: 'top', 'bottom' (the band then goes up), 'none'
                fontMm: 4.5,    // the size of the browser's own font of the labels, millimetres, when the layout names none
                zoom: 0,        // screen pixels a pixel of the display; 0: as fits
                page: 0,        // the page shown
                row: 0,         // the key chosen: its row and place in it
                key: 0,
                mode: 'edit'    // or 'try': the keys type into the field
            },
            trial: { text: '', shift: false, page: 0, flash: '', pressed: -1 },
            fontsLoaded: 0,  // how many fonts LoadedFonts has: what the font of the labels depends on
            history: [],
            future: [],
            message: ''
        };
    },
    getters: {
        page(state) {
            return state.layout.pages[Math.min(state.edit.mode === 'try' ? state.trial.page : state.edit.page, state.layout.pages.length - 1)];
        },
        pageIndex(state) {
            return Math.min(state.edit.mode === 'try' ? state.trial.page : state.edit.page, state.layout.pages.length - 1);
        },
        // the program's field, in pixels of the display: where the text goes
        fieldRect(state) {
            const d = state.edit.display;
            if (state.edit.field === 'none') return null;
            const mm = KeyboardLayout.pixels(10, d.height, d.heightMm) || 1;
            const h = Math.max(Math.round(this.labelFont.px * 1.6), 8 * mm), x = Math.round(d.width * 0.05);
            return { x, y: state.edit.field === 'top' ? 4 * mm : d.height - h - 4 * mm, w: d.width - 2 * x, h };
        },
        band(state) {
            return KeyboardLayout.place(state.layout, state.edit.display, this.fieldRect);
        },
        keys() {
            return this.band ? KeyboardLayout.lay(this.layout, this.band, this.pageIndex) : [];
        },
        chosen(state) {
            const row = this.page.rows[state.edit.row];
            return row ? row[state.edit.key] : undefined;
        },
        // the font the labels are drawn in: the layout's if it is loaded here, else the browser's own at its size
        labelFont(state) {
            const want = state.layout.font, loaded = state.fontsLoaded >= 0 && LoadedFonts.get(fontIdentity(want));
            const d = state.edit.display, mm = want && !want.dgx ? want.mm || +(want.px * d.widthMm / d.width).toFixed(1) : state.edit.fontMm || 4.5;
            const px = want && !want.dgx ? KeyboardLayout.fontPx(want, d) : KeyboardLayout.mmToPx(mm, d);
            if (loaded && loaded.kind === 'dgx') return { kind: 'dgx', dgx: loaded.dgx, has: loaded.has, px: loaded.dgx.size };
            if (loaded) return { kind: loaded.kind, css: `"${loaded.css}"`, has: loaded.has, px, mm };
            return { kind: 'browser', css: BrowserFont, has: null, px, mm, missing: !!want };
        },
        // the keys whose labels have letters the font has not: [{page, row, key, chars}]
        fontLacking() {
            const has = this.labelFont.has;
            return has ? KeyboardFonts.lacking(this.layout, has) : [];
        },
        problems(state) {
            const out = KeyboardLayout.problems(state.layout, state.edit.display), want = state.layout.font;
            if (want && this.labelFont.missing) out.push({ level: 'warning', text: `the font ${want.dgx || [want.family, want.style].filter(Boolean).join(' ')} is not opened here: the labels in the browser's` });
            for (const l of this.fontLacking) {
                const page = state.layout.pages[l.page];
                out.push({ level: 'warning', text: `page ${l.page + 1} (${page.name || 'no name'}), row ${l.row + 1}, key ${l.key + 1}: the font has no ${l.chars.join(' ')}` });
            }
            return out;
        }
    },
    actions: {
        // a change of the layout, undone by undo: what changes it (what it gives back is not looked at)
        change(what) {
            this.remember();
            what(this.layout);
            this.keepChoice();
            this.save();
        },
        // another layout instead, undone by undo
        replace(layout) {
            this.remember();
            this.layout = layout;
            this.keepChoice();
            this.save();
            this.restoreFont();
        },
        remember() {
            this.history.push(JSON.stringify(this.layout));
            if (this.history.length > 200) this.history.shift();
            this.future = [];
        },
        undo() {
            if (!this.history.length) return;
            this.future.push(JSON.stringify(this.layout));
            this.layout = JSON.parse(this.history.pop());
            this.keepChoice();
            this.save();
            this.restoreFont();
        },
        redo() {
            if (!this.future.length) return;
            this.history.push(JSON.stringify(this.layout));
            this.layout = JSON.parse(this.future.pop());
            this.keepChoice();
            this.save();
            this.restoreFont();
        },
        // the page and the key chosen still there
        keepChoice() {
            const e = this.edit, pages = this.layout.pages;
            e.page = Math.max(0, Math.min(e.page, pages.length - 1));
            const rows = pages[e.page].rows;
            e.row = Math.max(0, Math.min(e.row, rows.length - 1));
            e.key = Math.max(0, Math.min(e.key, rows[e.row].length - 1));
            this.trial.page = Math.min(this.trial.page, pages.length - 1);
        },
        choose(row, key) {
            this.edit.row = row, this.edit.key = key;
            this.save();
        },
        showPage(p) {
            this.edit.page = p, this.edit.row = 0, this.edit.key = 0;
            this.save();
        },
        setDisplay(display) {
            this.edit.display = { ...display };
            this.save();
        },
        save() {
            try {
                localStorage.setItem(StorageKey, JSON.stringify({ layout: this.layout, edit: this.edit }));
            } catch (e) {
                this.message = 'Not kept in the browser: ' + e.message;
            }
        },
        restore() {
            try {
                const saved = JSON.parse(localStorage.getItem(StorageKey) || 'null');
                if (saved && !KeyboardLayout.check(saved.layout)) {
                    this.layout = saved.layout;
                    Object.assign(this.edit, saved.edit || {});
                    this.keepChoice();
                }
            } catch (e) {
                this.message = 'What the browser kept is not read: ' + e.message;
            }
        },
        // a layout of a file, or the reason it is not taken
        open(text, name) {
            let json;
            try {
                json = JSON.parse(text);
            } catch (e) {
                return this.message = `${name}: not JSON (${e.message})`;
            }
            const wrong = KeyboardLayout.check(json);
            if (wrong) return this.message = `${name}: ${wrong}`;
            this.edit.page = 0, this.edit.row = 0, this.edit.key = 0;
            this.replace(json);
            this.trial = { text: '', shift: false, page: 0, flash: '', pressed: -1 };
            this.keepChoice();
            this.message = `${name}: opened`;
        },
        // --- the font of the labels: the browser's own, a font of the computer, of a file, a font of DGX

        // the size of the labels' font, millimetres (a tenth a step); a layout of before loses its pixels
        setFontMm(mm) {
            if (!(mm > 0)) return;
            const want = this.layout.font, tenths = Math.round(mm * 10) / 10;
            if (want && !want.dgx) this.change(l => { l.font.mm = tenths; delete l.font.px; });
            else this.edit.fontMm = tenths, this.save();
        },
        useBrowserFont() {
            this.change(l => { delete l.font; });
            this.message = 'the labels in the browser\'s own font';
        },
        // a font of DGX of its C (font2c) or JSON (dgx_ge_font.py): the labels drawn in it pixel for pixel
        useDgxFont(source, name) {
            let dgx;
            try {
                dgx = typeof source === 'string' && !source.trimStart().startsWith('{') ? KeyboardFonts.fromC(source) : KeyboardFonts.fromJson(typeof source === 'string' ? JSON.parse(source) : source);
            } catch (e) {
                return this.message = `${name}: ${e.message}`;
            }
            LoadedFonts.set('dgx:' + dgx.name, { kind: 'dgx', dgx, has: cp => dgx.glyphs.has(cp) });
            ++this.fontsLoaded;
            try {
                localStorage.setItem(FontStorageKey, typeof source === 'string' ? source : JSON.stringify(source));
            } catch (e) {
                localStorage.removeItem(FontStorageKey);
            }
            this.change(l => { l.font = { dgx: dgx.name }; });
            this.message = `${name}: ${dgx.name}, a font of DGX, ${dgx.glyphs.size} letters, ${dgx.size} px a line: the labels as DGX draws them`;
        },
        // a font file's data (a TTF, an OTF, a collection: of the face named), as a font of the page; its names and
        // letters as its tables tell, if it tells
        async useFontData(buffer, kind, name, postscript) {
            let face = { names: {}, has: null };
            try {
                face = KeyboardFonts.sfnt(buffer, postscript);
            } catch (e) {
                // a WOFF or a font of another kind: shown, its letters not known
            }
            const css = `kb-label-${++fontAliases}`, names = face.names;
            try {
                const f = new FontFace(css, buffer);
                await f.load();
                document.fonts.add(f);
            } catch (e) {
                return this.message = `${name}: the browser does not take it as a font (${e.message})`;
            }
            const want = { family: names.family || name.replace(/\.[^.]*$/, ''), style: names.style || '', postscript: names.postscript || postscript || '' };
            LoadedFonts.set(fontIdentity(want), { kind, css, has: face.has });
            ++this.fontsLoaded;
            const d = this.edit.display, was = this.layout.font && !this.layout.font.dgx ? this.layout.font : null;
            const mm = was ? was.mm || Math.round(was.px * d.widthMm / d.width * 10) / 10 : this.edit.fontMm || 4.5;
            this.change(l => { l.font = { ...want, mm }; });
            this.message = `${name}: ${want.family} ${want.style}` + (face.has ? '' : ', its letters not known') + (kind === 'file' ? '; its file is found again for the build by its names' : '');
        },
        // a file of a font: by its name, a font of DGX (.c; .json, if not a layout) or a font file
        async openFontFile(file) {
            if (/\.c$/i.test(file.name)) return this.useDgxFont(await file.text(), file.name);
            if (/\.json$/i.test(file.name)) {
                const text = await file.text();
                let json = null;
                try {
                    json = JSON.parse(text);
                } catch (e) {
                    return this.message = `${file.name}: not JSON (${e.message})`;
                }
                return json && json.format === KeyboardLayout.Format.name ? this.open(text, file.name) : this.useDgxFont(json, file.name);
            }
            return this.useFontData(await file.arrayBuffer(), 'file', file.name);
        },
        // the fonts of the computer (Chrome and Edge, once allowed; by a click): [FontData], or none and why
        async computerFonts() {
            if (typeof window.queryLocalFonts !== 'function') {
                this.message = 'this browser lists no fonts of the computer (Chrome and Edge do); a font file opens in any';
                return [];
            }
            try {
                return await window.queryLocalFonts();
            } catch (e) {
                this.message = `the fonts of the computer not given: ${e.message}`;
                return [];
            }
        },
        async useComputerFont(data) {
            return this.useFontData(await (await data.blob()).arrayBuffer(), 'system', data.fullName, data.postscriptName);
        },
        // the layout's font loaded again (after a visit, a file, undo): a font of DGX from what the browser kept, a face of
        // the computer by its PostScript name (its letters, if the fonts of the computer are allowed already)
        async restoreFont() {
            const want = this.layout.font, id = fontIdentity(want);
            if (!want || LoadedFonts.has(id)) return;
            if (want.dgx) {
                try {
                    const kept = localStorage.getItem(FontStorageKey);
                    const dgx = kept && (kept.trimStart().startsWith('{') ? KeyboardFonts.fromJson(JSON.parse(kept)) : KeyboardFonts.fromC(kept));
                    if (dgx && dgx.name === want.dgx) {
                        LoadedFonts.set(id, { kind: 'dgx', dgx, has: cp => dgx.glyphs.has(cp) });
                        return ++this.fontsLoaded;
                    }
                } catch (e) {
                    // not kept, or not of that font
                }
                return this.message = `${want.dgx}: open its C (font2c) again to see the labels in it`;
            }
            if (!want.postscript) return;
            try {
                if (typeof window.queryLocalFonts === 'function') {
                    const [data] = await window.queryLocalFonts({ postscriptNames: [want.postscript] });
                    if (data) {
                        let face = { has: null };
                        try {
                            face = KeyboardFonts.sfnt(await (await data.blob()).arrayBuffer(), want.postscript);
                        } catch (e) {
                            // shown all the same
                        }
                        const css = `kb-label-${++fontAliases}`, f = new FontFace(css, `local("${want.postscript}")`);
                        await f.load();
                        document.fonts.add(f);
                        LoadedFonts.set(id, { kind: 'system', css, has: face.has });
                        return ++this.fontsLoaded;
                    }
                }
            } catch (e) {
                // the fonts of the computer not allowed yet: by CSS's local() below
            }
            try {
                const css = `kb-label-${++fontAliases}`, f = new FontFace(css, `local("${want.postscript}")`);
                await f.load();
                document.fonts.add(f);
                LoadedFonts.set(id, { kind: 'system', css, has: null });
                ++this.fontsLoaded;
            } catch (e) {
                this.message = `${want.family} ${want.style}: not on this computer, the labels in the browser's own font`;
            }
        },
        // a layout of a template to begin with (KeyboardLayout.Templates; the code of digits if none is named)
        newLayout(name) {
            const t = KeyboardLayout.Templates.find(t => t.name === name) || KeyboardLayout.Templates[0];
            this.edit.page = 0, this.edit.row = 0, this.edit.key = 0;
            this.trial = { text: '', shift: false, page: 0, flash: '', pressed: -1 };
            this.replace(t.make());
            this.message = `${t.label}: ${t.about}, to begin with`;
        },
        // the keys: the chosen one changed, moved, another put by it, it taken away
        setKey(fields) {
            this.change(layout => {
                const key = layout.pages[this.edit.page].rows[this.edit.row][this.edit.key];
                for (const [name, value] of Object.entries(fields)) {
                    if (value === undefined) delete key[name];
                    else key[name] = value;
                }
            });
        },
        moveKey(step) {
            const row = this.page.rows[this.edit.row], to = this.edit.key + step;
            if (to < 0 || to >= row.length) return;
            this.change(layout => {
                const r = layout.pages[this.edit.page].rows[this.edit.row];
                [r[this.edit.key], r[to]] = [r[to], r[this.edit.key]];
            });
            this.edit.key = to;
        },
        addKey(after) {
            const at = this.edit.key + (after ? 1 : 0);
            this.change(layout => layout.pages[this.edit.page].rows[this.edit.row].splice(at, 0, { char: '?' }));
            this.edit.key = at;
        },
        removeKey() {
            const rows = this.page.rows;
            if (rows[this.edit.row].length === 1 && rows.length === 1) return this.message = 'a page keeps one key at least';
            this.change(layout => {
                const page = layout.pages[this.edit.page];
                page.rows[this.edit.row].splice(this.edit.key, 1);
                if (!page.rows[this.edit.row].length) page.rows.splice(this.edit.row, 1);
            });
        },
        // the rows: a new one by the chosen, the chosen moved or taken away
        addRow(after) {
            const at = this.edit.row + (after ? 1 : 0);
            this.change(layout => layout.pages[this.edit.page].rows.splice(at, 0, [{ char: '?' }]));
            this.edit.row = at, this.edit.key = 0;
        },
        moveRow(step) {
            const to = this.edit.row + step;
            if (to < 0 || to >= this.page.rows.length) return;
            this.change(layout => {
                const rows = layout.pages[this.edit.page].rows;
                [rows[this.edit.row], rows[to]] = [rows[to], rows[this.edit.row]];
            });
            this.edit.row = to;
        },
        removeRow() {
            if (this.page.rows.length === 1) return this.message = 'a page keeps one row at least';
            this.change(layout => layout.pages[this.edit.page].rows.splice(this.edit.row, 1));
        },
        // the pages: a new one after the shown, the shown renamed or taken away (the keys to the pages after it follow)
        addPage() {
            const at = this.edit.page + 1;
            this.change(layout => {
                for (const page of layout.pages)
                    for (const row of page.rows)
                        for (const key of row)
                            if (key.page !== undefined && key.page >= at) key.page += 1;
                layout.pages.splice(at, 0, { name: 'page ' + (at + 1), rows: [[{ page: this.edit.page, label: 'back', width: 6 }, { char: '?' }]] });
            });
            this.showPage(at);
        },
        removePage() {
            if (this.layout.pages.length === 1) return this.message = 'a layout keeps one page at least';
            const gone = this.edit.page;
            this.change(layout => {
                layout.pages.splice(gone, 1);
                for (const page of layout.pages)
                    for (const row of page.rows)
                        for (const key of row)
                            if (key.page !== undefined && key.page > gone) key.page -= 1;
                            else if (key.page === gone) key.page = 0;
            });
            this.showPage(Math.max(0, gone - 1));
        },
        // the trial: a key of the page shown let go, as DGX-DI takes it
        type(index) {
            const t = this.trial, laid = this.keys[index];
            if (!laid) return;
            const key = laid.key, code = KeyboardLayout.code(key);
            t.flash = '';
            if (key.action === 'shift') t.shift = !t.shift;
            else if (key.page !== undefined) {
                if (key.page < this.layout.pages.length) t.page = key.page, t.shift = false;
            } else if (key.action === 'erase') t.text = [...t.text].slice(0, -1).join('');
            else if (key.action === 'done') t.flash = 'done';
            else if (key.action === 'cancel') t.text = '', t.flash = 'cancel';
            else {
                t.text += String.fromCodePoint(KeyboardLayout.typed(key, t.shift));
                if (t.shift && code >= 0x20) t.shift = false;
            }
        },
        erase() {
            this.trial.text = [...this.trial.text].slice(0, -1).join('');
        }
    }
});
