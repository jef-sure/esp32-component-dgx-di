// Small parts of the workbench: the icons; a thumbnail of a layout on a display and of items in a look, drawn as the
// display draws them; the anchor an item's place is measured from, as a grid of nine; the readout of the pad under a
// finger in Try.
const PadIcons = (() => {
    const svg = (body, box = 16) => `<svg viewBox="0 0 ${box} ${box}" width="1em" height="1em" fill="none" stroke="currentColor" stroke-width="1.6" stroke-linecap="round" stroke-linejoin="round" aria-hidden="true">${body}</svg>`;
    return {
        button: svg('<circle cx="8" cy="8" r="6"/>'),
        cross: svg('<path d="M6 2h4v4h4v4h-4v4H6v-4H2V6h4z"/>'),
        stick: svg('<circle cx="8" cy="8" r="6.5"/><circle cx="9.5" cy="6.5" r="2.5" fill="currentColor" stroke="none"/>'),
        select: svg('<path d="M3 2l10 6-4.5 1.2L6.5 14z" fill="currentColor"/>'),
        circle: svg('<circle cx="8" cy="8" r="6" fill="currentColor" stroke="none"/>'),
        ring: svg('<circle cx="8" cy="8" r="5.5" stroke-width="3"/>'),
        rect: svg('<rect x="2" y="3.5" width="12" height="9" rx="2" fill="currentColor" stroke="none"/>'),
        polygon: svg('<path d="M8 2l6 4.5-2.3 7H4.3L2 6.5z" fill="currentColor" stroke="none"/>'),
        line: svg('<path d="M2 12c3-8 5 6 8-2 1-3 2-3 4-4" stroke-width="2.2"/>'),
        pencil: svg('<path d="M3 13l1-4 7-7 3 3-7 7z"/><path d="M10 3l3 3"/>'),
        copy: svg('<rect x="5" y="5" width="9" height="9" rx="1.5"/><path d="M11 5V3a1 1 0 0 0-1-1H3a1 1 0 0 0-1 1v7a1 1 0 0 0 1 1h2"/>'),
        trash: svg('<path d="M3 4h10M6 4V2.5h4V4M4.5 4l.7 9.5h5.6l.7-9.5"/>'),
        up: svg('<path d="M8 13V3M4 7l4-4 4 4"/>'),
        down: svg('<path d="M8 3v10M4 9l4 4 4-4"/>'),
        help: svg('<circle cx="8" cy="8" r="6.5"/><path d="M6 6.3a2 2 0 1 1 3 1.7c-.7.4-1 .8-1 1.5"/><circle cx="8" cy="12" r=".6" fill="currentColor"/>'),
        close: svg('<path d="M4 4l8 8M12 4l-8 8"/>'),
        check: svg('<path d="M3 8.5l3.5 3.5L13 5"/>'),
        warning: svg('<path d="M8 2l6.5 11.5h-13z"/><path d="M8 6.5v3.5"/><circle cx="8" cy="12" r=".6" fill="currentColor"/>'),
        error: svg('<circle cx="8" cy="8" r="6.5"/><path d="M5.5 5.5l5 5M10.5 5.5l-5 5"/>'),
        image: svg('<rect x="2" y="3" width="12" height="10" rx="1.5"/><circle cx="6" cy="7" r="1.3"/><path d="M14 11l-3.5-3.5L5 13"/>'),
        grid: svg('<path d="M2 6h12M2 10h12M6 2v12M10 2v12"/>'),
        back: svg('<path d="M13 8H3M7 4L3 8l4 4"/>'),
        download: svg('<path d="M8 2v8M4.5 7L8 10.5 11.5 7M3 13h10"/>'),
        folder: svg('<path d="M2 4.5A1.5 1.5 0 0 1 3.5 3h3l1.5 1.5h4.5A1.5 1.5 0 0 1 14 6v6a1.5 1.5 0 0 1-1.5 1.5h-9A1.5 1.5 0 0 1 2 12z"/>'),
        plus: svg('<path d="M8 3v10M3 8h10"/>'),
        hand: svg('<path d="M5 8V3.5a1 1 0 0 1 2 0V7m0-4.5a1 1 0 0 1 2 0V7m0-3a1 1 0 0 1 2 0v4m0-2.5a1 1 0 0 1 2 0V10c0 2.5-2 4.5-4.5 4.5S4 12.5 4 10.5v-2a1 1 0 0 1 1-.5z"/>'),
        move: svg('<path d="M8 2v12M2 8h12M6 4l2-2 2 2M6 12l2 2 2-2M4 6L2 8l2 2M12 6l2 2-2 2"/>'),
        undo: svg('<path d="M4 7h6a3 3 0 0 1 0 6H7"/><path d="M6.5 4.5L4 7l2.5 2.5"/>'),
        redo: svg('<path d="M12 7H6a3 3 0 0 0 0 6h3"/><path d="M9.5 4.5L12 7l-2.5 2.5"/>'),
        revert: svg('<path d="M3 8a5 5 0 1 1 1.5 3.5"/><path d="M3 4v4h4"/>')
    };
})();

// a canvas drawn once per change of what it shows: draw(ctx, canvas) is given by the one who uses it
const PadThumbMixin = {
    watch: { key() { this.$nextTick(this.paint); } },
    mounted() { this.paint(); },
    methods: {
        paint() {
            const canvas = this.$refs.canvas;
            if (canvas) this.draw(canvas.getContext('2d'), canvas);
        }
    }
};

// a layout on a display, small: the items as DGX-DI draws them, over a dark ground
const PadThumb = {
    mixins: [PadThumbMixin],
    props: { layout: Object, display: Object, width: { type: Number, default: 160 } },
    computed: {
        key() { return JSON.stringify([this.layout, this.display, this.width]); },
        height() { return Math.round(this.width * this.display.height / this.display.width); }
    },
    methods: {
        draw(ctx, canvas) {
            const d = this.display, full = document.createElement('canvas');
            full.width = d.width, full.height = d.height;
            const g = full.getContext('2d');
            g.fillStyle = '#2a3340';
            g.fillRect(0, 0, d.width, d.height);
            const pls = PadLayout.places(this.layout, d);
            this.layout.items.forEach((it, k) => PadLayout.drawItem(g, it, pls[k], d, null));
            canvas.width = this.width, canvas.height = this.height;
            ctx.imageSmoothingQuality = 'high';
            ctx.drawImage(full, 0, 0, this.width, this.height);
        }
    },
    template: `<canvas ref="canvas" class="pd-thumb" :width="width" :height="height"></canvas>`
};

// items in a row, each in a state, on a ground: a look (a theme) shown on a cross, a button and a stick, or one item
// in its own pictures (theme: none) or in a theme's
const ItemThumb = {
    mixins: [PadThumbMixin],
    props: { items: Array, theme: { type: String, default: null }, states: { type: Array, default: () => [] }, height: { type: Number, default: 44 }, ground: { type: String, default: '#2a3340' } },
    computed: {
        key() { return JSON.stringify([this.items, this.theme, this.states, this.height]); },
        // each item in a cell as wide as the largest: the thumb a cell a item
        cell() { return Math.max(...this.items.map(it => it.r)) * 2.3; },
        width() { return Math.round(this.height * this.items.length); }
    },
    methods: {
        draw(ctx, canvas) {
            const n = this.items.length, h = this.height, cell = this.cell, k = h / cell; // pixels a tenth of a millimetre
            canvas.width = h * n, canvas.height = h;
            ctx.fillStyle = this.ground;
            ctx.fillRect(0, 0, canvas.width, h);
            // a display of k pixels a tenth of a millimetre, each item in the middle of its cell
            const d = { width: Math.round(cell * n * k), height: Math.round(cell * k), widthMm: cell * n / 10, heightMm: cell / 10 };
            this.items.forEach((it, i) => {
                const item = { ...it };
                if (this.theme !== null) {
                    const p = PadLayout.Themes[this.theme](item);
                    if (p) item.pictures = p;
                    else delete item.pictures;
                }
                const pl = { x: Math.round(h * (i + 0.5)), y: Math.round(h / 2), r: Math.max(2, Math.round(it.r * k)) };
                // a state's knob is given as a part of the radius each way: in pixels here
                const st = this.states[i] || null;
                PadLayout.drawItem(ctx, item, pl, d, st && st.knob ? { ...st, knobX: Math.round(st.knob[0] * pl.r), knobY: Math.round(st.knob[1] * pl.r) } : st);
            });
        }
    },
    template: `<canvas ref="canvas" class="pd-thumb" :width="width" :height="height"></canvas>`
};

// the anchor of an item's place: which edge, or the middle, x and y are measured from
const AnchorPicker = {
    props: { fromX: { type: String, default: 'start' }, fromY: { type: String, default: 'start' } },
    emits: ['change'],
    data() {
        return { froms: ['start', 'middle', 'end'], names: { start: ['левого края', 'верха'], middle: ['середины', 'середины'], end: ['правого края', 'низа'] } };
    },
    template: `
    <div class="pd-anchor" title="От чего отсчитывать место элемента: от края или от середины дисплея (щёлкни клетку)">
        <template v-for="fy in froms" :key="fy"><button v-for="fx in froms" :key="fx" type="button" class="pd-anchor-cell" :class="{ chosen: fx === fromX && fy === fromY }"
            :title="'x от ' + names[fx][0] + ', y от ' + names[fy][1]" @click="$emit('change', { fromX: fx, fromY: fy })"></button></template>
    </div>`
};

// the readout of Try: the buttons held, lit; the sticks, their knobs where the finger has them
const PadTrial = {
    computed: {
        ...Pinia.mapStores(usePadStore),
        groups: () => PadLayout.BitGroups,
        buttons() { return this.padStore.trial.buttons; },
        axes() { return this.padStore.trial.axes; },
        // the sticks of the layout: each side once
        sticks() {
            const sides = new Set(this.padStore.layout.items.filter(it => it.kind === 'stick').map(it => it.out === 'right' ? 'right' : 'left'));
            return ['left', 'right'].filter(s => sides.has(s));
        },
        // the bits the layout can give at all: the others are grey
        given() {
            let m = 0;
            for (const it of this.padStore.layout.items) {
                if (it.kind === 'button') m |= PadLayout.bit(it.out);
                if (it.kind === 'cross') m |= PadLayout.bit('LEFT') | PadLayout.bit('RIGHT') | PadLayout.bit('UP') | PadLayout.bit('DOWN');
            }
            return m;
        }
    },
    methods: {
        lit(b) { return (this.buttons & PadLayout.bit(b)) !== 0; },
        can(b) { return (this.given & PadLayout.bit(b)) !== 0; },
        knob(side) {
            const a = this.axes && this.axes.side === side ? this.axes : { x: 0, y: 0 };
            return { left: 50 + a.x / 32767 * 40 + '%', top: 50 + a.y / 32767 * 40 + '%' };
        },
        value(side) {
            const a = this.axes && this.axes.side === side ? this.axes : null;
            return a ? `${a.x}, ${a.y}` : '0, 0';
        }
    },
    template: `
    <div class="pd-props pd-trial">
        <section>
            <h6>Try it</h6>
            <p class="pd-help-text">Мышь — палец, как DGX-DI его возьмёт: нажимай кнопки, веди по крестовине (углы — две стороны) и по стику. Что получает программа — ниже.</p>
        </section>
        <section>
            <h6>Buttons <span class="pd-note">bits of dgx_di_pad_t</span></h6>
            <div v-for="g in groups" :key="g.name" class="pd-bits">
                <span v-for="b in g.bits" :key="b" class="pd-bit" :class="{ lit: lit(b), absent: !can(b) }" :title="can(b) ? b : b + ': такой кнопки в раскладке нет'">{{ g.labels ? g.labels[b] : b }}</span>
            </div>
        </section>
        <section v-if="sticks.length">
            <h6>Sticks <span class="pd-note">axes, −32767..32767</span></h6>
            <div class="pd-sticks">
                <div v-for="s in sticks" :key="s" class="pd-stick-box">
                    <div class="pd-stick-well"><span class="pd-stick-knob" :class="{ lit: axes && axes.side === s }" :style="knob(s)"></span></div>
                    <div class="pd-note">{{ s }}: {{ value(s) }}</div>
                </div>
            </div>
        </section>
        <section>
            <p class="pd-help-text">Кнопка держится в полутора радиусах от середины; палец, уведённый дальше, её отпускает. Крестовина даёт ту сторону, что нарисована под пальцем; у середины — ничего. Стик отдаёт оси по расстоянию от середины, на ободе — всё.</p>
        </section>
    </div>`
};
