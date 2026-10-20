// The properties on the right: the display and what lies under the pad, the layer's alpha; the themes; the items, the
// chosen one's kind, button, place, size, color and pictures; what is wrong with the layout.
const PadProps = {
    data() {
        return { themes: Object.keys(PadLayout.Themes) };
    },
    computed: {
        ...Pinia.mapStores(usePadStore),
        displays() {
            return PadLayout.Displays;
        },
        presetName: {
            get() {
                const d = this.padStore.edit.display, p = PadLayout.Displays.find(p => p.width === d.width && p.height === d.height && p.widthMm === d.widthMm && p.heightMm === d.heightMm);
                return p ? p.name : '';
            },
            set(name) {
                const p = PadLayout.Displays.find(p => p.name === name);
                if (p) this.padStore.edit.display = { ...p }, this.padStore.save();
            }
        },
        item() {
            return this.padStore.chosen;
        },
        place() {
            return this.padStore.places[this.padStore.edit.item];
        },
        weight() {
            return PadLayout.weightOf(this.padStore.edit.alpha);
        },
        bits() {
            return PadLayout.Bits;
        },
        pictureNames() {
            return this.item ? PadLayout.Pictures[this.item.kind] : [];
        }
    },
    methods: {
        setDisplayField(name, value) {
            const v = parseInt(value, 10);
            if (v > 0 && v <= 8192) this.padStore.edit.display = { ...this.padStore.edit.display, name: 'custom', [name]: v }, this.padStore.save();
        },
        mm(name) {
            const v = this.item[name];
            return ((v || 0) / 10).toFixed(1);
        },
        setMm(name, value) {
            const v = Math.round(parseFloat(value) * 10);
            if (!Number.isFinite(v)) return;
            if (name === 'r') this.padStore.resizeItem(this.padStore.edit.item, v);
            else this.padStore.setItem({ [name]: v });
        },
        setKind(kind) {
            const it = this.item, out = kind === 'button' ? (PadLayout.Bits.includes(it.out) ? it.out : 'A') : kind === 'stick' ? (it.out === 'right' ? 'right' : 'left') : undefined;
            this.padStore.setItem({ kind, out, four: kind === 'cross' ? it.four : undefined, floating: kind === 'stick' ? it.floating : undefined, pictures: undefined });
        },
        loadImage(event) {
            const file = event.target.files[0];
            event.target.value = '';
            if (!file) return;
            const img = new Image();
            img.onload = () => {
                this.padStore.image = img, this.padStore.edit.background = 'image';
                this.padStore.message = `${file.name} under the pad`;
            };
            img.src = URL.createObjectURL(file);
        },
        label(it, k) {
            return `${k + 1}. ${it.kind}${it.out ? ' ' + it.out : ''}${it.pictures ? ' ✎' : ''}`;
        }
    },
    template: `
    <div class="pd-props">
        <section>
            <h6 title="Дисплей, на котором видна раскладка: его пиксели и миллиметры, как он стоит в программе">Display</h6>
            <select class="form-select form-select-sm mb-1" v-model="presetName">
                <option value="" disabled>custom</option>
                <option v-for="d in displays" :key="d.name" :value="d.name">{{ d.name }}</option>
            </select>
            <div class="pd-dims"><span>px</span><input type="number" class="form-control form-control-sm" :value="padStore.edit.display.width" @change="setDisplayField('width', $event.target.value)"><span>×</span><input type="number" class="form-control form-control-sm" :value="padStore.edit.display.height" @change="setDisplayField('height', $event.target.value)"></div>
            <div class="pd-dims"><span>mm</span><input type="number" class="form-control form-control-sm" :value="padStore.edit.display.widthMm" @change="setDisplayField('widthMm', $event.target.value)"><span>×</span><input type="number" class="form-control form-control-sm" :value="padStore.edit.display.heightMm" @change="setDisplayField('heightMm', $event.target.value)"></div>
            <label class="pd-row" title="Что под геймпадом: придуманная картинка игры, тёмное, светлое или своя картинка">under it
                <select class="form-select form-select-sm" v-model="padStore.edit.background" @change="padStore.save()">
                    <option value="scene">a game made up</option><option value="dark">dark</option><option value="light">light</option><option value="image" :disabled="!padStore.image">the image</option>
                </select>
                <label class="btn btn-outline-secondary btn-sm mb-0" title="Своя картинка под геймпадом (PNG, JPEG): например, кадр своей игры">image…<input type="file" accept="image/*" hidden @change="loadImage"></label>
            </label>
            <label class="pd-row" :title="'Насколько слой виден поверх картинки, как dgx_di_pad_show() его берёт: 0..255; DGX смешивает по ' + weight + ' из 32'">alpha <input type="range" min="0" max="255" class="form-range" v-model.number="padStore.edit.alpha" @change="padStore.save()"> {{ padStore.edit.alpha }}</label>
        </section>
        <section>
            <h6 title="Тема — рисунки всем элементам сразу (Default — без рисунков, как DGX-DI рисует сам); у одного элемента — кнопкой ниже">Theme</h6>
            <div class="pd-presets"><button v-for="t in themes" :key="t" class="btn btn-outline-secondary btn-sm" :title="'Тема ' + t + ' всем элементам'" @click="padStore.theme(t, true)">{{ t }}</button></div>
        </section>
        <section>
            <h6 title="Элементы раскладки; первый в списке берёт палец, где два перекрываются">Items</h6>
            <div class="pd-items">
                <div v-for="(it, k) in padStore.layout.items" :key="k" class="pd-item" :class="{ chosen: k === padStore.edit.item }" @click="padStore.choose(k)">
                    <span class="pd-swatch" :style="{ background: it.color || '#5a5a5a' }"></span>{{ label(it, k) }}
                </div>
            </div>
            <div class="btn-group btn-group-sm mt-1">
                <button class="btn btn-outline-secondary" title="Новая кнопка посередине" @click="padStore.addItem('button')">+ button</button>
                <button class="btn btn-outline-secondary" title="Новая крестовина посередине" @click="padStore.addItem('cross')">+ cross</button>
                <button class="btn btn-outline-secondary" title="Новый стик посередине" @click="padStore.addItem('stick')">+ stick</button>
                <button class="btn btn-outline-danger" title="Убрать выбранный элемент" @click="padStore.removeItem()">✕</button>
            </div>
        </section>
        <section v-if="item">
            <h6>Item {{ padStore.edit.item + 1 }} <span class="pd-note" v-if="place">{{ place.x }}, {{ place.y }}, r {{ place.r }} px</span></h6>
            <div class="pd-grid2">
                <label title="Вид элемента">kind <select class="form-select form-select-sm" :value="item.kind" @change="setKind($event.target.value)"><option v-for="k in ['button', 'cross', 'stick']" :key="k" :value="k">{{ k }}</option></select></label>
                <label v-if="item.kind === 'button'" title="Кнопка — бит движка (dgx_ge_input.h)">button <select class="form-select form-select-sm" :value="item.out" @change="padStore.setItem({ out: $event.target.value })"><option v-for="b in bits" :key="b" :value="b">{{ b }}</option></select></label>
                <label v-if="item.kind === 'stick'" title="Левый или правый стик: его оси">stick <select class="form-select form-select-sm" :value="item.out || 'left'" @change="padStore.setItem({ out: $event.target.value })"><option value="left">left</option><option value="right">right</option></select></label>
                <label v-if="item.kind === 'cross'" title="Только четыре стороны: из угла — луч, к которому ближе"><input type="checkbox" :checked="item.four" @change="padStore.setItem({ four: $event.target.checked })"> four ways</label>
                <label v-if="item.kind === 'stick'" title="Середина стика там, где опустился палец"><input type="checkbox" :checked="item.floating" @change="padStore.setItem({ floating: $event.target.checked })"> floating</label>
                <label title="От чего отсчитывать x: левый край, середина, правый край (к середине дисплея)">from x <select class="form-select form-select-sm" :value="item.fromX || 'start'" @change="padStore.setItem({ fromX: $event.target.value })"><option value="start">left</option><option value="middle">middle</option><option value="end">right</option></select></label>
                <label title="От чего отсчитывать y: верх, середина, низ">from y <select class="form-select form-select-sm" :value="item.fromY || 'start'" @change="padStore.setItem({ fromY: $event.target.value })"><option value="start">top</option><option value="middle">middle</option><option value="end">bottom</option></select></label>
                <label title="Отступ от края (или середины) по x, мм">x, mm <input type="number" step="0.5" class="form-control form-control-sm" :value="mm('x')" @change="setMm('x', $event.target.value)"></label>
                <label title="Отступ от края (или середины) по y, мм">y, mm <input type="number" step="0.5" class="form-control form-control-sm" :value="mm('y')" @change="setMm('y', $event.target.value)"></label>
                <label title="Радиус: круг, за который его берёт палец; рисунки растут с ним">radius, mm <input type="number" step="0.5" class="form-control form-control-sm" :value="mm('r')" @change="setMm('r', $event.target.value)"></label>
                <label title="Цвет элемента: фигуры цвета «item», «light», «dark» берут его">color <input type="color" class="form-control form-control-sm form-control-color" :value="item.color || '#5a5a5a'" @change="padStore.setItem({ color: $event.target.value })"></label>
            </div>
            <div class="mt-1"><span class="pd-note me-1">pictures</span>
                <div class="btn-group btn-group-sm">
                    <button v-for="n in pictureNames" :key="n" class="btn btn-outline-primary" :title="'Рисунок ' + n + ': открыть'" @click="padStore.editPicture(n)">{{ n }}<span v-if="item.pictures && item.pictures[n]"> {{ item.pictures[n].length }}</span></button>
                </div>
            </div>
            <div class="mt-1">
                <select class="form-select form-select-sm d-inline-block w-auto" title="Тема только этому элементу" @change="padStore.theme($event.target.value, false); $event.target.value = ''">
                    <option value="">theme of this one…</option><option v-for="t in themes" :key="t" :value="t">{{ t }}</option>
                </select>
            </div>
        </section>
        <section v-if="padStore.problems.length">
            <h6 title="Ошибки — pad_blob.py такую раскладку не возьмёт; предупреждения — возьмёт, но, может быть, не так, как задумано">Problems</h6>
            <div v-for="(p, i) in padStore.problems" :key="i" :class="p.level === 'error' ? 'pd-error' : 'pd-warning'" class="pd-problem">{{ p.text }}</div>
        </section>
    </div>`
};
