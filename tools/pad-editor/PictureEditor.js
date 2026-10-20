// A picture of the chosen item, large: a grid of millimetres, the circle a finger takes the item by, the picture under
// it where there is one (an arm held over the cross's normal, a knob over the well), its shapes; the chosen shape
// outlined. Tools: choose and move, a circle (from its middle out), a ring, a rectangle (corner to corner), a polygon
// (a click a point, a double click ends it), a line drawn by hand. Coordinates are tenths of a millimetre from the
// item's middle, y down, as the file has them.
const PictureEditor = {
    data() {
        return { making: null, drag: null, view: 440 };
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
        }
    },
    watch: {
        drawKey() {
            this.draw();
        }
    },
    mounted() {
        this.draw();
        window.addEventListener('keydown', this.keys);
    },
    beforeUnmount() {
        window.removeEventListener('keydown', this.keys);
    },
    methods: {
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
            if (event.key === 'Escape') this.making = null;
            else if (event.key === 'Enter') this.finish();
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
        }
    },
    template: `
    <div class="pd-picture">
        <div class="pd-picture-head">
            <strong>Picture of {{ item.kind }}{{ item.out ? ' ' + item.out : '' }}:</strong>
            <div class="btn-group btn-group-sm">
                <button v-for="n in names" :key="n" class="btn" :class="n === padStore.edit.picture ? 'btn-primary' : 'btn-outline-primary'" @click="padStore.editPicture(n)">{{ n }}</button>
            </div>
            <span class="ms-auto"></span>
            <button class="btn btn-outline-secondary btn-sm" title="Эти рисунки — всем элементам того же вида, с поправкой на их размер (цвет «item» у каждого свой)" @click="padStore.picturesToKind()">to every {{ item.kind }}</button>
            <button v-if="item.kind === 'cross'" class="btn btn-outline-secondary btn-sm" title="Нажатые лучи вправо, вниз, влево — повёрнутый нажатый луч вверх" @click="padStore.armsFromUp()">arms from up</button>
            <button class="btn btn-outline-danger btn-sm" title="Стереть все фигуры этого рисунка" @click="padStore.clearPicture()">clear</button>
            <button class="btn btn-secondary btn-sm" title="Закрыть рисунок" @click="padStore.edit.picture = ''">close</button>
        </div>
        <div class="pd-picture-body">
            <div class="pd-tools">
                <button v-for="t in ['select', 'circle', 'ring', 'rect', 'polygon', 'line']" :key="t" class="btn btn-sm" :class="t === padStore.edit.tool ? 'btn-dark' : 'btn-outline-dark'" @click="tool(t)"
                    :title="{ select: 'Выбрать фигуру и двигать', circle: 'Круг: от середины наружу', ring: 'Кольцо: от середины наружу, толщина — ниже', rect: 'Прямоугольник: из угла в угол', polygon: 'Многоугольник: щелчок — точка, двойной щелчок или Enter — конец (выпуклый)', line: 'Линия от руки: вести с нажатой кнопкой' }[t]">{{ t }}</button>
                <label class="pd-row mt-2" title="Цвет новых фигур (и выбранной): цвет элемента, светлее, темнее или свой">color
                    <select class="form-select form-select-sm" :value="padStore.edit.paint.startsWith('#') ? 'own' : padStore.edit.paint" @change="setPaint($event.target.value === 'own' ? '#ffffff' : $event.target.value)">
                        <option value="item">item</option><option value="light">light</option><option value="dark">dark</option><option value="own">own</option>
                    </select></label>
                <input v-if="padStore.edit.paint.startsWith('#')" type="color" class="form-control form-control-sm form-control-color" :value="padStore.edit.paint" @change="setPaint($event.target.value)">
                <label class="pd-row" title="Толщина новых колец и линий, мм">width, mm <input type="number" step="0.1" class="form-control form-control-sm" :value="(padStore.edit.width / 10).toFixed(1)" @change="padStore.edit.width = Math.max(1, Math.round($event.target.value * 10)); padStore.save()"></label>
            </div>
            <canvas ref="canvas" class="pd-picture-canvas" @mousedown.prevent="down" @mousemove="move" @mouseup="up" @dblclick="finish"></canvas>
            <div class="pd-shapes">
                <div v-for="(sh, i) in padStore.shapes" :key="i" class="pd-shape" :class="{ chosen: i === padStore.edit.shape }" @click="padStore.edit.shape = i">
                    <span class="pd-swatch" :style="{ background: paintOf(sh) }"></span>{{ i + 1 }}. {{ sh.shape }} <span class="pd-note">{{ sh.color || 'item' }}</span>
                </div>
                <div v-if="!padStore.shapes.length" class="pd-note">no shapes: draw some</div>
                <div v-if="shape" class="mt-2">
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
                        <button class="btn btn-outline-secondary" title="Ниже (раньше рисуется)" @click="padStore.orderShape(-1)">↓</button>
                        <button class="btn btn-outline-secondary" title="Выше (позже рисуется)" @click="padStore.orderShape(1)">↑</button>
                        <button class="btn btn-outline-secondary" title="Копия рядом" @click="duplicate">copy</button>
                        <button class="btn btn-outline-danger" title="Убрать фигуру (Delete)" @click="padStore.removeShape()">✕</button>
                    </div>
                </div>
            </div>
        </div>
    </div>`
};
