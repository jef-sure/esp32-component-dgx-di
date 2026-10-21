// The state of the pad editor: the layout edited (PadLayout's), how it is seen (the display, the picture under the
// pad, the layer's alpha, the grid), the item and the picture chosen, the trial of fingers; undo and redo; kept in the
// browser.
const PadStorageKey = 'dgx-di-pad-editor';

const usePadStore = Pinia.defineStore('pad', {
    state() {
        return {
            layout: PadLayout.classic(),
            // the layout as it was opened, begun of a template or saved as JSON: Revert goes back to it
            origin: { layout: PadLayout.classic(), from: 'the template Classic' },
            edit: {
                display: { ...PadLayout.Displays[0] },
                background: 'scene', // what lies under the pad: 'scene' (a game made up), 'dark', 'light', 'image' (one's own)
                alpha: 160,          // how much of the layer over it, 0..255, as dgx_di_pad_show() takes it
                zoom: 0,             // screen pixels a pixel of the display; 0: as fits
                grid: true,          // a grid of centimetres over the display while editing
                item: 0,             // the item chosen
                mode: 'edit',        // or 'try': the mouse a finger
                picture: '',         // the picture of the chosen item edited ('' none): normal, pressed, up...
                tool: 'select',      // circle, ring, rect, polygon, line
                shape: -1,           // the shape of the picture chosen
                paint: 'item',       // the color new shapes take: item, light, dark, #rrggbb
                width: 40            // tenths of a millimetre: of new rings and lines
            },
            image: null,             // the picture of one's own under the pad, an image (not kept)
            trial: { finger: null, buttons: 0, axes: null, states: {} },
            history: [],
            future: [],
            message: '',
            level: '',               // of the message: '' a note, 'ok' done, 'error' refused
            undoable: false          // the message tells of a change undo takes back: offer it
        };
    },
    getters: {
        places(state) {
            return PadLayout.places(state.layout, state.edit.display);
        },
        chosen(state) {
            return state.layout.items[state.edit.item];
        },
        problems(state) {
            return PadLayout.problems(state.layout, state.edit.display);
        },
        // the layout changed since it was opened (or begun, or saved): Revert has something to go back to
        changed(state) {
            return JSON.stringify(state.layout) !== JSON.stringify(state.origin.layout);
        },
        // the shapes of the picture edited
        shapes(state) {
            const it = state.layout.items[state.edit.item];
            return it && state.edit.picture && it.pictures ? it.pictures[state.edit.picture] || [] : [];
        }
    },
    actions: {
        // a word to the user, in the line of state
        say(text, level, undoable) {
            this.message = text, this.level = level || '', this.undoable = !!undoable;
        },
        // a change of the layout, undone by undo: what changes it (what it gives back is not looked at)
        change(what) {
            this.remember();
            what(this.layout);
            this.keepChoice();
            this.save();
        },
        replace(layout) {
            this.remember();
            this.layout = layout;
            this.keepChoice();
            this.save();
        },
        remember() {
            this.history.push(JSON.stringify(this.layout));
            if (this.history.length > 200) this.history.shift();
            this.future = [];
        },
        undo() {
            if (!this.history.length) return this.say('nothing to undo', '');
            this.future.push(JSON.stringify(this.layout));
            this.layout = JSON.parse(this.history.pop());
            this.keepChoice();
            this.save();
            this.say(`undone (${this.history.length} more to undo)`, 'ok');
        },
        redo() {
            if (!this.future.length) return this.say('nothing to redo', '');
            this.history.push(JSON.stringify(this.layout));
            this.layout = JSON.parse(this.future.pop());
            this.keepChoice();
            this.save();
            this.say('redone', 'ok');
        },
        keepChoice() {
            const e = this.edit, items = this.layout.items;
            e.item = Math.max(0, Math.min(e.item, items.length - 1));
            const it = items[e.item];
            if (!it || !PadLayout.Pictures[it.kind].includes(e.picture)) e.picture = '';
            if (e.shape >= this.shapes.length) e.shape = this.shapes.length - 1;
        },
        // the layout and how it is seen kept between visits; undo and redo kept while the tab lives (a reload keeps them)
        save() {
            try {
                localStorage.setItem(PadStorageKey, JSON.stringify({ layout: this.layout, edit: this.edit, origin: this.origin }));
                sessionStorage.setItem(PadStorageKey, JSON.stringify({ history: this.history, future: this.future }));
            } catch (e) {
                this.say('Not kept in the browser: ' + e.message, 'error');
            }
        },
        restore() {
            try {
                const saved = JSON.parse(localStorage.getItem(PadStorageKey) || 'null');
                if (saved && !PadLayout.check(saved.layout)) {
                    this.layout = saved.layout;
                    Object.assign(this.edit, saved.edit || {}, { mode: 'edit' });
                    if (this.edit.background === 'image') this.edit.background = 'scene';
                    this.keepChoice();
                    if (saved.origin && saved.origin.layout && !PadLayout.check(saved.origin.layout)) this.origin = saved.origin;
                    else this.origin = { layout: JSON.parse(JSON.stringify(saved.layout)), from: 'the last visit' };
                }
                const steps = JSON.parse(sessionStorage.getItem(PadStorageKey) || 'null');
                if (steps && Array.isArray(steps.history) && Array.isArray(steps.future)) this.history = steps.history, this.future = steps.future;
            } catch (e) {
                this.say('What the browser kept is not read: ' + e.message, 'error');
            }
        },
        open(text, name) {
            let json;
            try {
                json = JSON.parse(text);
            } catch (e) {
                return this.say(`${name}: not JSON (${e.message})`, 'error');
            }
            const wrong = PadLayout.check(json);
            if (wrong) return this.say(`${name}: ${wrong}`, 'error');
            this.edit.item = 0, this.edit.picture = '', this.edit.shape = -1;
            this.replace(json);
            this.setOrigin(`the file ${name}`);
            this.say(`${name}: opened`, 'ok');
        },
        // the layout as it is now is what Revert goes back to
        setOrigin(from) {
            this.origin = { layout: JSON.parse(JSON.stringify(this.layout)), from };
            this.save();
        },
        // back to the layout as it was opened, begun or saved: one step, undone as one
        revert() {
            if (!this.changed) return this.say(`as ${this.origin.from} already`, '');
            this.edit.item = 0, this.edit.picture = '', this.edit.shape = -1;
            this.replace(JSON.parse(JSON.stringify(this.origin.layout)));
            this.say(`back to ${this.origin.from}`, 'ok', true);
        },
        // a layout of a template to begin with (PadLayout.Templates; the classic one if none is named)
        newLayout(name) {
            const t = PadLayout.Templates.find(t => t.name === name) || PadLayout.Templates[0];
            this.edit.item = 0, this.edit.picture = '', this.edit.shape = -1;
            this.replace(t.make());
            this.setOrigin(`the template ${t.label}`);
            this.say(`${t.label}: ${t.about}. Move the items, change their look, try it, then save`, 'ok', true);
        },
        setDisplay(d) {
            this.edit.display = { ...d };
            this.save();
        },
        choose(k) {
            this.edit.item = k, this.edit.picture = '', this.edit.shape = -1;
            this.save();
        },
        // items: one more of a kind in the middle, the chosen taken away, moved to a pixel, made larger or smaller
        addItem(kind) {
            const it = kind === 'button' ? { kind, fromX: 'middle', fromY: 'middle', x: 0, y: 0, r: 70, out: 'HOME' } :
                kind === 'cross' ? { kind, fromX: 'middle', fromY: 'middle', x: 0, y: 0, r: 200 } : { kind, fromX: 'middle', fromY: 'middle', x: 0, y: 0, r: 180, out: 'right' };
            this.change(l => l.items.push(it));
            this.choose(this.layout.items.length - 1);
            this.say(`${PadLayout.nameOf(it)} added in the middle: drag it where it belongs`, 'ok');
        },
        // a copy of the chosen one beside it, toward the middle of the display
        duplicateItem() {
            const k = this.edit.item, it = JSON.parse(JSON.stringify(this.layout.items[k]));
            it.x = (it.x || 0) + 2 * it.r + 20;
            this.change(l => l.items.splice(k + 1, 0, it));
            this.choose(k + 1);
            this.say(`a copy of ${PadLayout.nameOf(it)} beside it`, 'ok');
        },
        removeItem() {
            if (this.layout.items.length === 1) return this.say('a pad keeps one item at least', 'error');
            const k = this.edit.item, name = PadLayout.nameOf(this.layout.items[k]);
            this.change(l => l.items.splice(k, 1));
            this.choose(Math.max(0, k - 1));
            this.say(`${name} taken away`, 'ok', true);
        },
        // the chosen item earlier or later in the list: the first has the finger where two overlap
        orderItem(step) {
            const k = this.edit.item, to = k + step;
            if (to < 0 || to >= this.layout.items.length) return;
            this.change(l => { [l.items[k], l.items[to]] = [l.items[to], l.items[k]]; });
            this.choose(to);
        },
        // a pixel of the display as the item's place in tenths of a millimetre, a millimetre each (a tenth: fine)
        moveItem(k, px, py, fine, noUndo) {
            const it = this.layout.items[k], back = PadLayout.unplace(it, this.edit.display, px, py);
            const snap = v => fine ? v : Math.round(v / 10) * 10;
            const apply = l => Object.assign(l.items[k], { x: snap(back.x), y: snap(back.y) });
            if (noUndo) apply(this.layout), this.save();
            else this.change(apply);
        },
        // the chosen item moved by tenths of a millimetre to the right and down on the display, whatever it is measured from
        nudgeItem(dx, dy) {
            const k = this.edit.item;
            this.change(l => {
                const it = l.items[k];
                it.x = (it.x || 0) + (it.fromX === 'end' ? -dx : dx);
                it.y = (it.y || 0) + (it.fromY === 'end' ? -dy : dy);
            });
        },
        resizeItem(k, r, noUndo) {
            r = Math.max(10, Math.min(2000, Math.round(r)));
            const apply = l => {
                const it = l.items[k], ratio = r / it.r;
                it.r = r;
                if (it.pictures) for (const name of Object.keys(it.pictures)) it.pictures[name] = PadLayout.scaled(it.pictures[name], ratio);
            };
            if (noUndo) apply(this.layout), this.save();
            else this.change(apply);
        },
        setItem(fields) {
            const k = this.edit.item;
            this.change(l => {
                const it = l.items[k];
                for (const [name, value] of Object.entries(fields)) {
                    if (value === undefined || value === false || value === '') delete it[name];
                    else it[name] = value;
                }
            });
        },
        // themes: pictures for every item, or for the chosen one
        theme(name, all) {
            const k = this.edit.item;
            this.change(l => {
                for (const [i, it] of l.items.entries()) {
                    if (!all && i !== k) continue;
                    const p = PadLayout.Themes[name](it);
                    if (p) it.pictures = p;
                    else delete it.pictures;
                }
            });
            this.edit.picture = '', this.edit.shape = -1;
            this.say(all ? `the look ${name} given to every item` : `the look ${name} given to ${PadLayout.nameOf(this.layout.items[k])}`, 'ok', true);
        },
        // pictures of the chosen item
        editPicture(name) {
            const k = this.edit.item;
            if (!this.layout.items[k].pictures) this.change(l => { l.items[k].pictures = PadLayout.Themes.Flat(l.items[k]) || {}; });
            this.edit.picture = name, this.edit.shape = -1, this.edit.tool = 'select';
            this.save();
        },
        closePicture() {
            this.edit.picture = '', this.edit.shape = -1;
            this.save();
        },
        addShape(sh) {
            const k = this.edit.item, name = this.edit.picture;
            this.change(l => {
                const p = l.items[k].pictures || (l.items[k].pictures = {});
                (p[name] = p[name] || []).push(sh);
            });
            this.edit.shape = this.shapes.length - 1;
        },
        // the chosen shape changed; while dragged, the first change only is undone as one
        setShape(fields, noUndo) {
            const k = this.edit.item, name = this.edit.picture, i = this.edit.shape;
            const apply = l => Object.assign(l.items[k].pictures[name][i], fields);
            if (noUndo) apply(this.layout), this.save();
            else this.change(apply);
        },
        removeShape() {
            const k = this.edit.item, name = this.edit.picture, i = this.edit.shape;
            if (i < 0) return;
            this.change(l => l.items[k].pictures[name].splice(i, 1));
            this.edit.shape = Math.min(i, this.shapes.length - 1);
        },
        orderShape(step) {
            const k = this.edit.item, name = this.edit.picture, i = this.edit.shape, to = i + step;
            if (i < 0 || to < 0 || to >= this.shapes.length) return;
            this.change(l => {
                const s = l.items[k].pictures[name];
                [s[i], s[to]] = [s[to], s[i]];
            });
            this.edit.shape = to;
        },
        clearPicture() {
            const k = this.edit.item, name = this.edit.picture;
            this.change(l => { l.items[k].pictures[name] = []; });
            this.edit.shape = -1;
        },
        // the chosen item's pictures given to every item of its kind, made as much larger or smaller as they are
        picturesToKind() {
            const from = this.layout.items[this.edit.item];
            this.change(l => {
                for (const it of l.items) {
                    if (it === l.items[this.edit.item] || it.kind !== from.kind) continue;
                    if (!from.pictures) delete it.pictures;
                    else {
                        it.pictures = {};
                        for (const [name, shapes] of Object.entries(from.pictures)) it.pictures[name] = PadLayout.scaled(shapes, it.r / from.r);
                    }
                }
            });
            this.say(`the pictures given to every ${from.kind}`, 'ok', true);
        },
        // a cross's arm held up, turned to the other ways: right, down, left
        armsFromUp() {
            const k = this.edit.item;
            this.change(l => {
                const p = l.items[k].pictures, turn = (shapes, f) => (shapes || []).map(sh => {
                    const out = { ...sh };
                    if (sh.x !== undefined || sh.y !== undefined) [out.x, out.y] = f(sh.x || 0, sh.y || 0);
                    if (sh.shape === 'rect') [out.w, out.h] = f === turns.down ? [sh.w, sh.h] : [sh.h, sh.w];
                    if (sh.points) out.points = sh.points.map(([x, y]) => f(x, y));
                    return out;
                });
                const turns = { right: (x, y) => [-y, x], down: (x, y) => [-x, -y], left: (x, y) => [y, -x] };
                for (const way of ['right', 'down', 'left']) p[way] = turn(p.up, turns[way]);
            });
            this.say('the arms held right, down and left made of the one up', 'ok');
        },
        // the trial: a finger down at a pixel, moved, up
        fingerDown(x, y) {
            const pls = this.places, k = PadLayout.itemAt(pls, x, y);
            this.trial.finger = k >= 0 ? { item: k, x, y, downX: x, downY: y } : null;
            this.feel();
        },
        fingerMove(x, y) {
            if (!this.trial.finger) return;
            this.trial.finger.x = x, this.trial.finger.y = y;
            this.feel();
        },
        fingerUp() {
            this.trial.finger = null;
            this.feel();
        },
        feel() {
            const f = this.trial.finger;
            if (!f) return this.trial.buttons = 0, this.trial.axes = null, this.trial.states = {};
            const h = PadLayout.hold(this.layout.items[f.item], this.places[f.item], f.x, f.y, f.downX, f.downY);
            this.trial.buttons = h.buttons, this.trial.axes = h.axes, this.trial.states = { [f.item]: h.state };
        }
    }
});
