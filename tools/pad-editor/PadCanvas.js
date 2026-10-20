// The display in its own pixels: under the pad a picture (a game made up — sky, sun, hills, ground — or plain, or one's
// own image), as RGB565; over it the pad's layer, its items drawn as DGX-DI draws them (their pictures, or the default
// look), mixed over the picture as DGX mixes a layer at its alpha. The chosen item's circle, the one a finger takes it
// by, over all. Editing: an item dragged by its middle, made larger by its rim (a millimetre a step; Alt: a tenth).
// Trying: the mouse a finger.
const PadCanvas = {
    data() {
        return { drag: null, under: null, underKey: '' };
    },
    computed: {
        ...Pinia.mapStores(usePadStore),
        drawKey() {
            const s = this.padStore;
            return JSON.stringify([s.layout, s.edit.display, s.edit.background, s.edit.alpha, s.edit.item, s.edit.mode, s.trial.states, s.image ? s.image.src.length : 0]);
        },
        style() {
            const s = this.padStore, d = s.edit.display;
            return s.edit.zoom > 0 ? { width: d.width * s.edit.zoom + 'px' } : {};
        }
    },
    watch: {
        drawKey() {
            this.draw();
        }
    },
    mounted() {
        this.draw();
    },
    methods: {
        // the picture under the pad, made once for a display and a kind: RGB565 a pixel
        underneath() {
            const s = this.padStore, d = s.edit.display, key = [d.width, d.height, s.edit.background, s.image ? s.image.src : ''].join(',');
            if (this.under && this.underKey === key) return this.under;
            const c = document.createElement('canvas');
            c.width = d.width, c.height = d.height;
            const g = c.getContext('2d'), W = d.width, H = d.height;
            if (s.edit.background === 'image' && s.image) g.drawImage(s.image, 0, 0, W, H);
            else if (s.edit.background === 'dark' || s.edit.background === 'light') {
                g.fillStyle = s.edit.background === 'dark' ? '#14181e' : '#dfe3e8';
                g.fillRect(0, 0, W, H);
            } else {
                const sky = g.createLinearGradient(0, 0, 0, H * 0.62);
                sky.addColorStop(0, '#2b6cb0'), sky.addColorStop(1, '#f6c27a');
                g.fillStyle = sky;
                g.fillRect(0, 0, W, H);
                g.fillStyle = '#fff3b0';
                g.beginPath(), g.arc(W * 0.72, H * 0.3, H * 0.09, 0, 2 * Math.PI), g.fill();
                const hills = (color, base, peaks, seed) => {
                    g.fillStyle = color;
                    g.beginPath(), g.moveTo(0, H);
                    for (let i = 0; i <= peaks; ++i) g.lineTo(W * i / peaks, H * base - H * 0.12 * Math.abs(Math.sin(i * 1.7 + seed)));
                    g.lineTo(W, H), g.closePath(), g.fill();
                };
                hills('#5b6f8f', 0.6, 9, 0.4), hills('#3f5f4a', 0.7, 13, 2.1);
                g.fillStyle = '#6b4f2a';
                g.fillRect(0, H * 0.82, W, H * 0.18);
                g.fillStyle = '#8a6a3a';
                for (let x = 0; x < W; x += Math.max(8, Math.round(W / 40))) g.fillRect(x, H * 0.82, Math.max(4, Math.round(W / 80)), Math.max(3, Math.round(H / 120)));
            }
            const px = g.getImageData(0, 0, W, H).data, out = new Uint16Array(W * H);
            for (let i = 0; i < W * H; ++i) out[i] = (px[4 * i] & 0xf8) << 8 | (px[4 * i + 1] & 0xfc) << 3 | (px[4 * i + 2] & 0xf8) >> 3;
            this.under = out, this.underKey = key;
            return out;
        },
        draw() {
            const s = this.padStore, d = s.edit.display, canvas = this.$refs.canvas;
            if (!canvas) return;
            const W = d.width, H = d.height, under = this.underneath();
            canvas.width = W, canvas.height = H;
            // the layer: the items drawn, each in its state
            const layer = document.createElement('canvas');
            layer.width = W, layer.height = H;
            const l = layer.getContext('2d'), pls = s.places;
            s.layout.items.forEach((it, k) => PadLayout.drawItem(l, it, pls[k], d, s.edit.mode === 'try' ? s.trial.states[k] : null));
            // mixed over what lies under it, as DGX mixes: a pixel of the layer drawn (more than half) over the picture
            const lp = l.getImageData(0, 0, W, H).data, ctx = canvas.getContext('2d'), out = ctx.createImageData(W, H), o = out.data;
            const weight = PadLayout.weightOf(s.edit.alpha);
            for (let i = 0; i < W * H; ++i) {
                let c = under[i];
                if (lp[4 * i + 3] >= 128) c = PadLayout.mix(c, (lp[4 * i] & 0xf8) << 8 | (lp[4 * i + 1] & 0xfc) << 3 | (lp[4 * i + 2] & 0xf8) >> 3, weight);
                const r = c >> 11 & 31, g = c >> 5 & 63, b = c & 31;
                o[4 * i] = r << 3 | r >> 2, o[4 * i + 1] = g << 2 | g >> 4, o[4 * i + 2] = b << 3 | b >> 2, o[4 * i + 3] = 255;
            }
            ctx.putImageData(out, 0, 0);
            // the chosen item's circle and handles
            const p = pls[s.edit.item];
            if (p && s.edit.mode === 'edit') {
                const line = Math.max(1, Math.round(W / 500));
                ctx.lineWidth = line;
                ctx.setLineDash([6 * line, 4 * line]);
                ctx.strokeStyle = '#ffcc00';
                ctx.beginPath(), ctx.arc(p.x + 0.5, p.y + 0.5, p.r, 0, 2 * Math.PI), ctx.stroke();
                ctx.setLineDash([]);
                ctx.fillStyle = '#ffcc00';
                ctx.fillRect(p.x - 3 * line, p.y - 3 * line, 6 * line, 6 * line);
                ctx.beginPath(), ctx.arc(p.x + p.r, p.y, 4 * line, 0, 2 * Math.PI), ctx.fill();
            }
        },
        at(event) {
            const canvas = this.$refs.canvas, r = canvas.getBoundingClientRect();
            const scale = Math.min(r.width / canvas.width, r.height / canvas.height);
            const left = r.left + (r.width - canvas.width * scale) / 2, top = r.top + (r.height - canvas.height * scale) / 2;
            return { x: Math.floor((event.clientX - left) / scale), y: Math.floor((event.clientY - top) / scale), scale };
        },
        down(event) {
            const s = this.padStore, p = this.at(event), pls = s.places;
            if (s.edit.mode === 'try') return s.fingerDown(p.x, p.y);
            // the rim of the chosen one first, then any item
            const c = pls[s.edit.item], grip = Math.max(6, 8 / p.scale);
            if (c && Math.abs(Math.hypot(p.x - c.x, p.y - c.y) - c.r) <= grip) {
                this.drag = { kind: 'rim', k: s.edit.item, r0: s.layout.items[s.edit.item].r, d0: Math.hypot(p.x - c.x, p.y - c.y) || 1 };
                return;
            }
            const k = PadLayout.itemAt(pls, p.x, p.y);
            if (k < 0) return;
            s.choose(k);
            this.drag = { kind: 'move', k, dx: p.x - pls[k].x, dy: p.y - pls[k].y };
        },
        move(event) {
            const s = this.padStore, p = this.at(event);
            if (s.edit.mode === 'try') return s.fingerMove(p.x, p.y);
            if (!this.drag) return;
            if (!this.drag.moved) s.remember(), this.drag.moved = true; /* one undo the whole drag */
            if (this.drag.kind === 'move') s.moveItem(this.drag.k, p.x - this.drag.dx, p.y - this.drag.dy, event.altKey, true);
            else {
                const c = s.places[this.drag.k], d = Math.hypot(p.x - c.x, p.y - c.y), r = this.drag.r0 * d / this.drag.d0;
                s.resizeItem(this.drag.k, event.altKey ? r : Math.round(r / 10) * 10, true);
            }
        },
        up() {
            const s = this.padStore;
            if (s.edit.mode === 'try') return s.fingerUp();
            this.drag = null;
        }
    },
    template: `
    <div class="pd-canvas-box">
        <canvas ref="canvas" class="pd-canvas" :class="{ 'pd-fit': !padStore.edit.zoom }" :style="style" @mousedown.prevent="down" @mousemove="move" @mouseup="up" @mouseleave="up"
            :title="padStore.edit.mode === 'try' ? 'Проба: мышь — палец: нажимай кнопки, веди по крестовине и стику' : 'Щелчок выбирает элемент; тянуть за середину — двигать, за обод — менять размер (миллиметр за шаг, с Alt — десятая)'"></canvas>
    </div>`
};
