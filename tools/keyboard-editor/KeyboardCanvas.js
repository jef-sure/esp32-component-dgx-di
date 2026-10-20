// The display in its own pixels: the program's screen (a grid of centimetres, its field), the band of the keyboard and
// its keys drawn as DGX-DI draws them (the colors as RGB565 makes them, the band's colors mixed row by row as the C mixes
// them, the signs of the same pieces); the labels in a font of the browser, of the size the program's DGX font has.
// Editing: a click chooses a key. Trying: the keys type into the field, erase held again and again.
const KeyboardCanvas = {
    data() {
        return { repeat: null, repeated: false };
    },
    computed: {
        ...Pinia.mapStores(useKeyboardStore),
        // what the picture depends on
        drawKey() {
            const s = this.keyboardStore;
            return JSON.stringify([s.layout, s.edit, s.trial]);
        },
        style() {
            const s = this.keyboardStore, d = s.edit.display;
            return s.edit.zoom > 0 ? { width: d.width * s.edit.zoom + 'px', maxWidth: 'none', maxHeight: 'none' } : {};
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
    beforeUnmount() {
        this.stopRepeat();
    },
    methods: {
        draw() {
            const s = this.keyboardStore, d = s.edit.display, canvas = this.$refs.canvas;
            if (!canvas) return;
            canvas.width = d.width, canvas.height = d.height;
            const ctx = canvas.getContext('2d'), L = KeyboardLayout, trying = s.edit.mode === 'try';
            // the program's screen: dark, a line every centimetre
            ctx.fillStyle = '#0c0f13';
            ctx.fillRect(0, 0, d.width, d.height);
            ctx.fillStyle = 'rgba(255, 255, 255, 0.07)';
            for (let mm = 10; mm < d.widthMm; mm += 10) ctx.fillRect(Math.round(mm * d.width / d.widthMm), 0, 1, d.height);
            for (let mm = 10; mm < d.heightMm; mm += 10) ctx.fillRect(0, Math.round(mm * d.height / d.heightMm), d.width, 1);
            ctx.font = `${s.edit.fontPx}px "DejaVu Sans", Verdana, sans-serif`;
            const cap = Math.round(ctx.measureText('H').actualBoundingBoxAscent || s.edit.fontPx * 0.73);
            const field = s.fieldRect;
            if (field) {
                const t = s.trial;
                ctx.fillStyle = t.flash === 'done' ? '#16351f' : t.flash === 'cancel' ? '#3a1616' : '#1b2129';
                ctx.fillRect(field.x, field.y, field.w, field.h);
                ctx.fillStyle = '#5c6b7d';
                ctx.fillRect(field.x, field.y + field.h - 2, field.w, 2);
                ctx.fillStyle = '#e8eef5';
                ctx.textAlign = 'left';
                const text = trying ? t.text + '|' : 'the program\'s field';
                ctx.fillText(text, field.x + Math.round(s.edit.fontPx / 2), field.y + Math.round(field.h / 2 + cap / 2), field.w - s.edit.fontPx);
            }
            const band = s.band;
            if (!band) {
                ctx.fillStyle = '#ff6b6b';
                ctx.textAlign = 'center';
                ctx.fillText('the keys do not fit this display', d.width / 2, d.height / 2);
                return;
            }
            const theme = s.layout.theme, c565 = name => L.rgb565(theme[name]);
            const top = c565('backTop'), bottom = c565('backBottom');
            for (let i = 0; i < band.h; ++i) {
                ctx.fillStyle = L.hex565(top === bottom ? top : L.mix565(top, bottom, i, band.h > 1 ? band.h - 1 : 1));
                ctx.fillRect(band.x, band.y + i, band.w, 1);
            }
            const color = name => L.hex565(c565(name)), shift = trying && s.trial.shift;
            s.keys.forEach((k, i) => {
                const code = L.code(k.key), plain = !L.special(code) || (k.key.action === 'shift' && shift);
                ctx.fillStyle = color(trying && s.trial.pressed === i ? 'keyPressed' : plain ? 'key' : 'keySpecial');
                this.rounded(ctx, k.x, k.y, k.w, k.h, band.radius);
                const label = L.label(k.key, shift), cx = k.x + Math.trunc(k.w / 2), cy = k.y + Math.trunc(k.h / 2);
                ctx.fillStyle = ctx.strokeStyle = color('label');
                if (label) {
                    ctx.textAlign = 'center';
                    ctx.fillText(label, cx, cy + Math.round(cap / 2));
                } else if (!k.key.label) this.sign(ctx, code, cx, cy, k.h);
                if (!trying && k.row === s.edit.row && k.index === s.edit.key && s.pageIndex === s.edit.page) {
                    ctx.strokeStyle = '#ffcc00';
                    ctx.lineWidth = Math.max(2, Math.round(d.width / 400));
                    ctx.strokeRect(k.x + 1, k.y + 1, k.w - 2, k.h - 2);
                }
            });
        },
        // a key's shape: rounded() of the C
        rounded(ctx, x, y, w, h, r) {
            r = Math.min(r, Math.trunc(w / 2), Math.trunc(h / 2));
            if (r < 1) return ctx.fillRect(x, y, w, h);
            ctx.fillRect(x + r, y, w - 2 * r, h);
            ctx.fillRect(x, y + r, r, h - 2 * r);
            ctx.fillRect(x + w - r, y + r, r, h - 2 * r);
            for (const [ox, oy] of [[x + r, y + r], [x + w - 1 - r, y + r], [x + r, y + h - 1 - r], [x + w - 1 - r, y + h - 1 - r]]) {
                ctx.beginPath();
                ctx.arc(ox + 0.5, oy + 0.5, r + 0.5, 0, 2 * Math.PI);
                ctx.fill();
            }
        },
        // the signs of erase, done, cancel and shift: sign() of the C, piece for piece
        sign(ctx, code, cx, cy, size) {
            const a = Math.max(Math.trunc(size / 4), 2), t = Math.max(Math.trunc(size / 16), 2), d = v => Math.trunc(v);
            const triangle = (x0, y0, x1, y1, x2, y2) => {
                ctx.beginPath();
                ctx.moveTo(x0 + 0.5, y0 + 0.5), ctx.lineTo(x1 + 0.5, y1 + 0.5), ctx.lineTo(x2 + 0.5, y2 + 0.5);
                ctx.closePath();
                ctx.fill();
            };
            const line = (x0, y0, x1, y1) => {
                ctx.lineWidth = t;
                ctx.beginPath();
                ctx.moveTo(x0 + 0.5, y0 + 0.5), ctx.lineTo(x1 + 0.5, y1 + 0.5);
                ctx.stroke();
            };
            if (code === 0x08) {
                triangle(cx - a, cy, cx - d(a / 4), cy - d(a * 3 / 4), cx - d(a / 4), cy + d(a * 3 / 4));
                ctx.fillRect(cx - d(a / 4), cy - d(t / 2), a + d(a / 4), t);
            } else if (code === 0x0a) {
                ctx.fillRect(cx + a - t, cy - a, t, a + d(t / 2));
                ctx.fillRect(cx - d(a / 2), cy - d(t / 2), a + d(a / 2), t);
                triangle(cx - a, cy, cx - d(a / 2), cy - d(a / 2), cx - d(a / 2), cy + d(a / 2));
            } else if (code === 0x1b) {
                line(cx - d(a * 3 / 4), cy - d(a * 3 / 4), cx + d(a * 3 / 4), cy + d(a * 3 / 4));
                line(cx - d(a * 3 / 4), cy + d(a * 3 / 4), cx + d(a * 3 / 4), cy - d(a * 3 / 4));
            } else if (code === KeyboardLayout.SHIFT) {
                triangle(cx, cy - a, cx - a, cy, cx + a, cy);
                ctx.fillRect(cx - d(a / 2), cy, a + 1, d(a * 3 / 4));
            }
        },
        // the pixel of the display under the mouse: the picture in the middle of the canvas, as large as fits (contain)
        at(event) {
            const canvas = this.$refs.canvas, r = canvas.getBoundingClientRect();
            const scale = Math.min(r.width / canvas.width, r.height / canvas.height);
            const left = r.left + (r.width - canvas.width * scale) / 2, top = r.top + (r.height - canvas.height * scale) / 2;
            return { x: Math.floor((event.clientX - left) / scale), y: Math.floor((event.clientY - top) / scale) };
        },
        keyAt(event) {
            const p = this.at(event);
            return this.keyboardStore.keys.findIndex(k => p.x >= k.x && p.x < k.x + k.w && p.y >= k.y && p.y < k.y + k.h);
        },
        down(event) {
            const s = this.keyboardStore, i = this.keyAt(event);
            if (s.edit.mode !== 'try') {
                if (i >= 0) s.choose(s.keys[i].row, s.keys[i].index);
                return;
            }
            s.trial.pressed = i, this.repeated = false;
            if (i >= 0 && s.keys[i].key.action === 'erase')
                this.repeat = setTimeout(() => {
                    s.erase(), this.repeated = true;
                    this.repeat = setInterval(() => s.erase(), 100);
                }, 500);
        },
        up(event) {
            const s = this.keyboardStore;
            if (s.edit.mode !== 'try') return;
            const i = this.keyAt(event);
            if (i >= 0 && i === s.trial.pressed && !this.repeated) s.type(i);
            this.release();
        },
        release() {
            this.keyboardStore.trial.pressed = -1;
            this.stopRepeat();
        },
        stopRepeat() {
            if (this.repeat) clearTimeout(this.repeat), clearInterval(this.repeat);
            this.repeat = null;
        }
    },
    template: `
    <div class="kb-canvas-box">
        <canvas ref="canvas" class="kb-canvas" :class="{ 'kb-fit': !keyboardStore.edit.zoom }" :style="style" @mousedown.prevent="down" @mouseup="up" @mouseleave="release"
            :title="keyboardStore.edit.mode === 'try' ? 'Проба: клавиши печатают в поле программы, стереть повторяется, если держать' : 'Щелчок выбирает клавишу: её свойства — справа'"></canvas>
    </div>`
};
