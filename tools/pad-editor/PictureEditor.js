// The pictures of the chosen item, in place of the display: each state of the item as a tab, with the item drawn in
// it; the picture chosen, large: a grid of millimetres, the circle a finger takes the item by, the picture under it
// where there is one (an arm held over the cross's normal, a knob over the well), its shapes; the chosen shape
// outlined. Beside it the item at the pixels of the display, as DGX-DI will draw it. Tools: choose and move, a circle
// (from its middle out), a ring, a rectangle (corner to corner), a polygon (a click a point, a double click ends it), a
// line drawn by hand. Coordinates are tenths of a millimetre from the item's middle, y down, as the file has them.
const PictureEditor = {
    data() {
        return {
            making: null, drag: null, view: 600, icons: PadIcons,
            tools: ['select', 'circle', 'ring', 'rect', 'polygon', 'line'],
            tips: { select: 'Выбрать фигуру щелчком и двигать её', circle: 'Круг: от середины наружу', ring: 'Кольцо: от середины наружу, толщина — ниже', rect: 'Прямоугольник: из угла в угол, скругление — ниже', polygon: 'Многоугольник (выпуклый): щелчок — точка, двойной щелчок или Enter — конец', line: 'Линия от руки: вести с нажатой кнопкой; толщина — ниже' },
            hints: { select: 'Click a shape to choose it, drag to move it · Delete takes it away · the numbers on the right place it exactly', circle: 'Press where the middle is and drag outward', ring: 'Press where the middle is and drag outward; its width is on the left', rect: 'Drag from one corner to the other', polygon: 'A click a point; a double click or Enter ends it, Escape drops it', line: 'Draw with the button held; a point each half millimetre' }
        };
    },
    computed: {
        ...Pinia.mapStores(usePadStore),
        item() {
            return this.padStore.chosen;
        },
        names() {
            return PadLayout.Pictures[this.item.kind];
        },
        // the picture drawn under the one edited
        base() {
            const n = this.padStore.edit.picture, k = this.item.kind;
            return k === 'cross' && n !== 'normal' ? 'normal' : k === 'stick' && n !== 'well' ? 'well' : '';
        },
        shape() {
            return this.padStore.shapes[this.padStore.edit.shape];
        },
        // tenths of a millimetre in pixels of the view: the item and a little around it
        k() {
            return this.view / (2.8 * this.item.r);
        },
        drawKey() {
            const s = this.padStore;
            return JSON.stringify([this.item, s.edit.picture, s.edit.shape, this.making]);
        },
        previewKey() {
            return JSON.stringify([this.item, this.padStore.edit.picture, this.padStore.edit.display]);
        },
        hint() {
            return this.hints[this.padStore.edit.tool];
        },
        about() {
            const k = this.item.kind;
            return k === 'button' ? 'A button is drawn by normal, and by pressed while a finger holds it.' :
                k === 'cross' ? 'A cross is drawn by normal; an arm held (up, down, left, right) is drawn over it, and a corner held (both ways) over them.' :
                    'A stick is drawn by its well; the knob over it, where the finger has it (half its way), knobHeld while held.';
        }
    },
    watch: {
        drawKey() {
            this.draw();
        },
        previewKey() {
            this.$nextTick(this.preview);
        }
    },
    mounted() {
        this.draw();
        this.preview();
        window.addEventListener('keydown', this.keys);
    },
    beforeUnmount() {
        window.removeEventListener('keydown', this.keys);
    },
    methods: {
        // the state of the item that shows a picture: for the tabs and the preview
        stateOf(name) {
            if (name === 'pressed' || name === 'knobHeld') return { down: true, knob: [0.25, 0] };
            const ways = { up: ['UP'], down: ['DOWN'], left: ['LEFT'], right: ['RIGHT'], upLeft: ['UP', 'LEFT'], upRight: ['UP', 'RIGHT'], downLeft: ['DOWN', 'LEFT'], downRight: ['DOWN', 'RIGHT'] }[name];
            return ways ? { ways: ways.reduce((m, w) => m | PadLayout.bit(w), 0) } : {};
        },
        // a view as large as one likes: a display of k pixels a tenth of a millimetre
        display() {
            const w = Math.max(1, Math.round(this.k * 10000));
            return { width: w, height: w, widthMm: 1000, heightMm: 1000 };
        },
        draw() {
            const canvas = this.$refs.canvas;
            if (!canvas) return;
            const v = this.view, c = v / 2, k = this.k, ctx = canvas.getContext('2d'), it = this.item, s = this.padStore;
            canvas.width = canvas.height = v;
            ctx.fillStyle = '#20252c';
            ctx.fillRect(0, 0, v, v);
            ctx.fillStyle = 'rgba(255, 255, 255, 0.06)';
            for (let t = 10; t * k < c; t += 10) {
                const a = Math.round(c + t * k), b = Math.round(c - t * k);
                ctx.fillRect(a, 0, 1, v), ctx.fillRect(b, 0, 1, v), ctx.fillRect(0, a, v, 1), ctx.fillRect(0, b, v, 1);
            }
            // the centimetres stronger, the axes strongest
            ctx.fillStyle = 'rgba(255, 255, 255, 0.1)';
            for (let t = 100; t * k < c; t += 100) {
                const a = Math.round(c + t * k), b = Math.round(c - t * k);
                ctx.fillRect(a, 0, 1, v), ctx.fillRect(b, 0, 1, v), ctx.fillRect(0, a, v, 1), ctx.fillRect(0, b, v, 1);
            }
            ctx.fillStyle = 'rgba(255, 255, 255, 0.18)';
            ctx.fillRect(Math.round(c), 0, 1, v), ctx.fillRect(0, Math.round(c), v, 1);
            const d = this.display(), p = it.pictures || {};
            if (this.base) PadLayout.drawPicture(ctx, it, p[this.base], c, c, d);
            PadLayout.drawPicture(ctx, it, s.shapes, c, c, d);
            if (this.making) PadLayout.drawPicture(ctx, it, [this.making], c, c, d);
            // the circle a finger takes it by
            ctx.setLineDash([6, 4]);
            ctx.strokeStyle = '#ffcc00';
            ctx.lineWidth = 1;
            ctx.beginPath(), ctx.arc(c, c, it.r * k, 0, 2 * Math.PI), ctx.stroke();
            ctx.setLineDash([]);
            // the chosen shape
            const sh = this.shape;
            if (sh) {
                const box = this.box(sh);
                ctx.strokeStyle = '#4dabf7';
                ctx.lineWidth = 2;
                ctx.strokeRect(c + box[0] * k - 3, c + box[1] * k - 3, (box[2] - box[0]) * k + 6, (box[3] - box[1]) * k + 6);
            }
        },
        // the item at the pixels of the display, in the state of the picture edited, twice as large
        preview() {
            const canvas = this.$refs.preview;
            if (!canvas) return;
            const s = this.padStore, d = s.edit.display, it = this.item, r = s.places[s.edit.item].r, size = 2 * r + 2 * Math.max(4, Math.round(r / 4));
            canvas.width = canvas.height = size;
            canvas.style.zoom = size <= 136 ? 2 : 1; /* a small one twice as large, pixel for pixel */
            const ctx = canvas.getContext('2d');
            ctx.fillStyle = '#2a3340';
            ctx.fillRect(0, 0, size, size);
            const st = this.stateOf(s.edit.picture), state = st.knob ? { ...st, knobX: Math.round(st.knob[0] * r), knobY: Math.round(st.knob[1] * r) } : st;
            PadLayout.drawItem(ctx, it, { x: Math.trunc(size / 2), y: Math.trunc(size / 2), r }, d, state);
        },
        // where a shape is, tenths of a millimetre: left, top, right, bottom
        box(sh) {
            if (sh.shape === 'circle' || sh.shape === 'ring') return [sh.x - sh.r, sh.y - sh.r, sh.x + sh.r, sh.y + sh.r];
            if (sh.shape === 'rect') return [sh.x - sh.w / 2, sh.y - sh.h / 2, sh.x + sh.w / 2, sh.y + sh.h / 2];
            const xs = sh.points.map(p => p[0]), ys = sh.points.map(p => p[1]), half = sh.shape === 'line' ? (sh.width || 1) / 2 : 0;
            return [Math.min(...xs) - half, Math.min(...ys) - half, Math.max(...xs) + half, Math.max(...ys) + half];
        },
        // the point under the mouse, tenths of a millimetre from the middle
        at(event) {
            const r = this.$refs.canvas.getBoundingClientRect(), scale = this.view / r.width;
            return { x: Math.round(((event.clientX - r.left) * scale - this.view / 2) / this.k), y: Math.round(((event.clientY - r.top) * scale - this.view / 2) / this.k) };
        },
        // the topmost shape at a point
        shapeAt(p) {
            const shapes = this.padStore.shapes;
            for (let i = shapes.length - 1; i >= 0; --i) {
                const sh = shapes[i], b = this.box(sh);
                if (p.x >= b[0] && p.x <= b[2] && p.y >= b[1] && p.y <= b[3]) return i;
            }
            return -1;
        },
        paint() {
            return this.padStore.edit.paint;
        },
        down(event) {
            const s = this.padStore, p = this.at(event), tool = s.edit.tool;
            if (tool === 'select') {
                const i = this.shapeAt(p);
                s.edit.shape = i;
                if (i >= 0) this.making = null, this.drag = { from: p, start: JSON.parse(JSON.stringify(s.shapes[i])), moved: false };
                return;
            }
            if (tool === 'polygon') {
                if (!this.making) this.making = { shape: 'polygon', points: [[p.x, p.y], [p.x, p.y]], color: this.paint() };
                else this.making.points.push([p.x, p.y]);
                return;
            }
            this.making = tool === 'line' ? { shape: 'line', points: [[p.x, p.y]], width: s.edit.width, color: this.paint() } :
                tool === 'rect' ? { shape: 'rect', x: p.x, y: p.y, w: 0, h: 0, r: 0, color: this.paint(), from: p } :
                    { shape: tool, x: p.x, y: p.y, r: 0, ...(tool === 'ring' ? { width: s.edit.width } : {}), color: this.paint() };
        },
        move(event) {
            const s = this.padStore, p = this.at(event);
            if (this.drag) {
                const dx = p.x - this.drag.from.x, dy = p.y - this.drag.from.y, st = this.drag.start;
                if (!dx && !dy) return;
                const fields = st.points ? { points: st.points.map(([x, y]) => [x + dx, y + dy]) } : { x: st.x + dx, y: st.y + dy };
                s.setShape(fields, this.drag.moved);
                this.drag.moved = true;
                return;
            }
            const m = this.making;
            if (!m || !(event.buttons & 1) && m.shape !== 'polygon') return;
            if (m.shape === 'circle' || m.shape === 'ring') m.r = Math.round(Math.hypot(p.x - m.x, p.y - m.y));
            else if (m.shape === 'rect') m.x = Math.round((m.from.x + p.x) / 2), m.y = Math.round((m.from.y + p.y) / 2), m.w = Math.abs(p.x - m.from.x), m.h = Math.abs(p.y - m.from.y);
            else if (m.shape === 'line') {
                const last = m.points[m.points.length - 1];
                if (Math.hypot(p.x - last[0], p.y - last[1]) >= 5) m.points.push([p.x, p.y]); /* a point each half millimetre */
            } else m.points[m.points.length - 1] = [p.x, p.y];
            this.making = { ...m };
        },
        up() {
            if (this.drag) return this.drag = null;
            const m = this.making;
            if (!m || m.shape === 'polygon') return;
            this.making = null;
            delete m.from;
            if ((m.shape === 'circle' || m.shape === 'ring') && m.r < 2) return;
            if (m.shape === 'rect' && (m.w < 2 || m.h < 2)) return;
            if (m.shape === 'line' && m.points.length < 2) return;
            this.padStore.addShape(m);
        },
        // a polygon ended: its last point, where the mouse went, dropped
        finish() {
            const m = this.making;
            if (!m || m.shape !== 'polygon') return;
            this.making = null;
            m.points.pop();
            if (m.points.length >= 3) this.padStore.addShape(m);
        },
        keys(event) {
            if (event.target.tagName === 'INPUT' || event.target.tagName === 'SELECT') return;
            if (event.key === 'Escape') {
                if (this.making) this.making = null;
                else this.padStore.closePicture();
            } else if (event.key === 'Enter') this.finish();
            else if ((event.key === 'Delete' || event.key === 'Backspace') && this.padStore.edit.shape >= 0) this.padStore.removeShape(), event.preventDefault();
        },
        tool(t) {
            this.making = null, this.padStore.edit.tool = t;
        },
        // the chosen shape's numbers, in millimetres
        mm(name) {
            const v = this.shape[name];
            return v === undefined ? '' : (v / 10).toFixed(1);
        },
        setMm(name, value) {
            const v = Math.round(parseFloat(value) * 10);
            if (Number.isFinite(v)) this.padStore.setShape({ [name]: name === 'x' || name === 'y' ? v : Math.max(0, v) });
        },
        setPaint(value) {
            this.padStore.edit.paint = value;
            if (this.shape) this.padStore.setShape({ color: value });
        },
        duplicate() {
            const sh = JSON.parse(JSON.stringify(this.shape));
            if (sh.points) sh.points = sh.points.map(([x, y]) => [x + 20, y + 20]);
            else sh.x += 20, sh.y += 20;
            this.padStore.addShape(sh);
        },
        paintOf(sh) {
            return PadLayout.hex565(PadLayout.shapeColor(this.item, sh));
        },
        count(n) {
            const p = this.item.pictures || {};
            return p[n] ? p[n].length : 0;
        }
    },
    template: `
    <div class="pd-picture">
        <div class="pd-picture-head">
            <button class="btn btn-primary btn-sm" title="Готово: назад к дисплею (Esc)" @click="padStore.closePicture()"><span v-html="icons.back"></span> Done</button>
            <strong>Pictures of {{ item.kind }}{{ item.out ? ' ' + item.out : '' }}</strong>
            <span class="pd-note">{{ about }}</span>
            <span class="ms-auto"></span>
            <button class="btn btn-outline-secondary btn-sm" title="Эти рисунки — всем элементам того же вида, с поправкой на их размер (цвет item у каждого свой)" @click="padStore.picturesToKind()">to every {{ item.kind }}</button>
            <button v-if="item.kind === 'cross'" class="btn btn-outline-secondary btn-sm" title="Нажатые лучи вправо, вниз, влево — повёрнутый нажатый луч вверх" @click="padStore.armsFromUp()">arms from the one up</button>
            <button class="btn btn-outline-danger btn-sm" title="Стереть все фигуры этого рисунка" @click="padStore.clearPicture()">clear this picture</button>
        </div>
        <div class="pd-states">
            <button v-for="n in names" :key="n" class="pd-state" :class="{ chosen: n === padStore.edit.picture }" :title="'Рисунок ' + n" @click="padStore.editPicture(n)">
                <ItemThumb :items="[item]" :states="[stateOf(n)]" :height="44"></ItemThumb><span>{{ n }}</span><span class="pd-note">{{ count(n) }}</span>
            </button>
        </div>
        <div class="pd-picture-body">
            <div class="pd-tools">
                <button v-for="t in tools" :key="t" class="btn btn-sm pd-tool" :class="t === padStore.edit.tool ? 'btn-dark' : 'btn-outline-dark'" :title="tips[t]" @click="tool(t)"><span v-html="icons[t]"></span>{{ t }}</button>
                <label class="pd-tool-field" title="Цвет новых фигур (и выбранной): цвет элемента, светлее, темнее или свой">color
                    <select class="form-select form-select-sm" :value="padStore.edit.paint.startsWith('#') ? 'own' : padStore.edit.paint" @change="setPaint($event.target.value === 'own' ? '#ffffff' : $event.target.value)">
                        <option value="item">item</option><option value="light">light</option><option value="dark">dark</option><option value="own">own…</option>
                    </select></label>
                <input v-if="padStore.edit.paint.startsWith('#')" type="color" class="form-control form-control-sm form-control-color" :value="padStore.edit.paint" @change="setPaint($event.target.value)">
                <label class="pd-tool-field" title="Толщина новых колец и линий, мм">width, mm <input type="number" step="0.1" min="0.1" class="form-control form-control-sm" :value="(padStore.edit.width / 10).toFixed(1)" @change="padStore.edit.width = Math.max(1, Math.round($event.target.value * 10)); padStore.save()"></label>
            </div>
            <div class="pd-picture-canvas-box"><canvas ref="canvas" class="pd-picture-canvas" @mousedown.prevent="down" @mousemove="move" @mouseup="up" @mouseleave="up" @dblclick="finish"></canvas></div>
            <div class="pd-picture-right">
                <div class="pd-preview" title="Так его нарисует DGX-DI на выбранном дисплее, в его пикселях (вдвое крупнее)">
                    <h6>On the display <span class="pd-note">{{ padStore.edit.picture }}, in its pixels</span></h6>
                    <canvas ref="preview" class="pd-preview-canvas"></canvas>
                </div>
                <div class="pd-shapes">
                    <h6>Shapes <span class="pd-note">drawn in this order</span></h6>
                    <div class="pd-shape-list">
                        <div v-for="(sh, i) in padStore.shapes" :key="i" class="pd-shape" :class="{ chosen: i === padStore.edit.shape }" @click="padStore.edit.shape = i">
                            <span class="pd-swatch" :style="{ background: paintOf(sh) }"></span><span v-html="icons[sh.shape]"></span> {{ sh.shape }} <span class="pd-note">{{ sh.color || 'item' }}</span>
                        </div>
                        <div v-if="!padStore.shapes.length" class="pd-note pd-empty">No shapes yet: pick a tool on the left and draw on the grid. The dashed circle is what a finger takes.</div>
                    </div>
                    <div v-if="shape" class="pd-shape-props">
                        <div class="pd-grid2">
                            <label v-if="!shape.points">x, mm <input type="number" step="0.1" class="form-control form-control-sm" :value="mm('x')" @change="setMm('x', $event.target.value)"></label>
                            <label v-if="!shape.points">y, mm <input type="number" step="0.1" class="form-control form-control-sm" :value="mm('y')" @change="setMm('y', $event.target.value)"></label>
                            <label v-if="shape.shape === 'circle' || shape.shape === 'ring' || shape.shape === 'rect'">{{ shape.shape === 'rect' ? 'corner' : 'radius' }}, mm <input type="number" step="0.1" class="form-control form-control-sm" :value="mm('r')" @change="setMm('r', $event.target.value)"></label>
                            <label v-if="shape.shape === 'ring' || shape.shape === 'line'">width, mm <input type="number" step="0.1" class="form-control form-control-sm" :value="mm('width')" @change="setMm('width', $event.target.value)"></label>
                            <label v-if="shape.shape === 'rect'">w, mm <input type="number" step="0.1" class="form-control form-control-sm" :value="mm('w')" @change="setMm('w', $event.target.value)"></label>
                            <label v-if="shape.shape === 'rect'">h, mm <input type="number" step="0.1" class="form-control form-control-sm" :value="mm('h')" @change="setMm('h', $event.target.value)"></label>
                        </div>
                        <div v-if="shape.points" class="pd-note">{{ shape.points.length }} points</div>
                        <div class="btn-group btn-group-sm mt-1">
                            <button class="btn btn-outline-secondary" title="Ниже: рисуется раньше, под другими" @click="padStore.orderShape(-1)" v-html="icons.up"></button>
                            <button class="btn btn-outline-secondary" title="Выше: рисуется позже, над другими" @click="padStore.orderShape(1)" v-html="icons.down"></button>
                            <button class="btn btn-outline-secondary" title="Копия рядом" @click="duplicate"><span v-html="icons.copy"></span> copy</button>
                            <button class="btn btn-outline-danger" title="Убрать фигуру (Delete)" @click="padStore.removeShape()" v-html="icons.trash"></button>
                        </div>
                    </div>
                </div>
            </div>
        </div>
        <div class="pd-canvas-hint"><span v-html="icons[padStore.edit.tool]"></span>{{ hint }}</div>
    </div>`
};
