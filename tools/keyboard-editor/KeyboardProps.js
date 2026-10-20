// The properties on the right: the display seen, the layout's sizes in millimetres, its colors, the key chosen, its
// row and page; what is wrong with the layout.
const KeyKinds = {
    char: 'a character',
    erase: 'erase (U+0008)',
    done: 'done (U+000A)',
    cancel: 'cancel (U+001B)',
    shift: 'shift',
    page: 'to a page'
};

const KeyboardProps = {
    data() {
        return { presets: ThemePresets, kinds: KeyKinds, colors: { backTop: 'band, top', backBottom: 'band, bottom', key: 'key', keySpecial: 'special key', keyPressed: 'pressed key', label: 'label' } };
    },
    computed: {
        ...Pinia.mapStores(useKeyboardStore),
        displays() {
            return KeyboardLayout.Displays;
        },
        presetName: {
            get() {
                const d = this.keyboardStore.edit.display, p = KeyboardLayout.Displays.find(p => p.width === d.width && p.height === d.height && p.widthMm === d.widthMm && p.heightMm === d.heightMm);
                return p ? p.name : '';
            },
            set(name) {
                const p = KeyboardLayout.Displays.find(p => p.name === name);
                if (p) this.keyboardStore.setDisplay(p);
            }
        },
        density() {
            const d = this.keyboardStore.edit.display;
            return `${(d.width / d.widthMm).toFixed(2)} × ${(d.height / d.heightMm).toFixed(2)} px a mm`;
        },
        key() {
            return this.keyboardStore.chosen;
        },
        kind() {
            const k = this.key;
            return !k ? '' : k.action || (k.page !== undefined ? 'page' : 'char');
        },
        // what the chosen key is in pixels
        laid() {
            const s = this.keyboardStore;
            return s.keys.find(k => k.row === s.edit.row && k.index === s.edit.key);
        },
        pageName: {
            get() {
                return this.keyboardStore.layout.pages[this.keyboardStore.edit.page].name || '';
            },
            set(v) {
                this.keyboardStore.change(l => { l.pages[this.keyboardStore.edit.page].name = v; });
            }
        },
        layoutName: {
            get() {
                return this.keyboardStore.layout.name || '';
            },
            set(v) {
                this.keyboardStore.change(l => { l.name = v; });
            }
        }
    },
    methods: {
        // a size of the layout, tenths of a millimetre, shown and given in millimetres
        mm(name) {
            return (this.keyboardStore.layout[name] / 10).toFixed(1);
        },
        setMm(name, value) {
            const v = Math.round(parseFloat(value) * 10);
            if (Number.isFinite(v) && v >= 0 && v <= 32767) this.keyboardStore.change(l => { l[name] = v; });
        },
        setDisplayField(name, value) {
            const v = parseInt(value, 10);
            if (v > 0 && v <= 8192) this.keyboardStore.setDisplay({ ...this.keyboardStore.edit.display, name: 'custom', [name]: v });
        },
        setColor(name, value) {
            this.keyboardStore.change(l => { l.theme[name] = value; });
        },
        // the color as the display shows it: RGB565
        shown(name) {
            return KeyboardLayout.hex565(KeyboardLayout.rgb565(this.keyboardStore.layout.theme[name]));
        },
        setTheme(name) {
            this.keyboardStore.change(l => { l.theme = { ...ThemePresets[name] }; });
        },
        setKind(kind) {
            const k = this.key;
            const fresh = kind === 'char' ? { char: k.char || '?', upper: k.upper } : kind === 'page' ? { page: k.page || 0, label: k.label || 'page' } : { action: kind };
            if (fresh.label === undefined) fresh.label = k.label;
            fresh.width = k.width;
            this.keyboardStore.change(l => {
                l.pages[this.keyboardStore.edit.page].rows[this.keyboardStore.edit.row][this.keyboardStore.edit.key] = KeyboardLayout.cleanKey(fresh);
            });
        },
        setChar(value) {
            const c = [...value][0];
            if (!c) return;
            const upper = c.toUpperCase();
            this.keyboardStore.setKey({ char: c, upper: [...upper].length === 1 && upper !== c ? upper : undefined });
        },
        setUpper(value) {
            const c = [...value][0];
            this.keyboardStore.setKey({ upper: c && c !== this.key.char ? c : undefined });
        },
        setLabel(value) {
            this.keyboardStore.setKey({ label: value === '' ? undefined : value });
        },
        setWidth(value) {
            const v = parseInt(value, 10);
            if (v >= 1 && v <= 255) this.keyboardStore.setKey({ width: v === 4 ? undefined : v });
        },
        setPage(value) {
            const v = parseInt(value, 10);
            if (v >= 0 && v < this.keyboardStore.layout.pages.length) this.keyboardStore.setKey({ page: v });
        },
        codeText(key) {
            const c = KeyboardLayout.code(key);
            return c >= KeyboardLayout.PAGE ? `page ${c - KeyboardLayout.PAGE + 1}` : c >= KeyboardLayout.SHIFT ? 'shift' : 'U+' + c.toString(16).toUpperCase().padStart(4, '0');
        }
    },
    template: `
    <div class="kb-props">
        <section>
            <h6 title="Дисплей, на котором видна раскладка: его пиксели и миллиметры, как он стоит в программе">Display</h6>
            <select class="form-select form-select-sm mb-1" v-model="presetName">
                <option value="" disabled>custom</option>
                <option v-for="d in displays" :key="d.name" :value="d.name">{{ d.name }}</option>
            </select>
            <div class="kb-dims" title="Пиксели по ширине и высоте, как дисплей стоит">
                <span>px</span><input type="number" class="form-control form-control-sm" :value="keyboardStore.edit.display.width" @change="setDisplayField('width', $event.target.value)">
                <span>×</span><input type="number" class="form-control form-control-sm" :value="keyboardStore.edit.display.height" @change="setDisplayField('height', $event.target.value)">
            </div>
            <div class="kb-dims" title="Миллиметры: измеренный размер видимой части дисплея">
                <span>mm</span><input type="number" class="form-control form-control-sm" :value="keyboardStore.edit.display.widthMm" @change="setDisplayField('widthMm', $event.target.value)">
                <span>×</span><input type="number" class="form-control form-control-sm" :value="keyboardStore.edit.display.heightMm" @change="setDisplayField('heightMm', $event.target.value)">
            </div>
            <div class="kb-note">{{ density }}</div>
            <div class="kb-grid2">
                <label title="Поле ввода программы: если полоса клавиатуры его закроет, она уходит наверх">field
                    <select class="form-select form-select-sm" v-model="keyboardStore.edit.field" @change="keyboardStore.save()">
                        <option value="top">at the top</option><option value="bottom">at the bottom</option><option value="none">none</option>
                    </select></label>
                <label title="Размер шрифта надписей в пикселях дисплея — как у шрифта DGX, который даст программа">font px <input type="number" class="form-control form-control-sm" v-model.number="keyboardStore.edit.fontPx" @change="keyboardStore.save()"></label>
            </div>
        </section>
        <section>
            <h6 title="Размеры раскладки в миллиметрах: на любом дисплее клавиша того же размера под пальцем">Layout</h6>
            <label class="kb-row">name <input class="form-control form-control-sm" :value="layoutName" @change="layoutName = $event.target.value"></label>
            <div class="kb-grid2">
                <label title="Высота клавиши">key height, mm <input type="number" step="0.1" class="form-control form-control-sm" :value="mm('keyH')" @change="setMm('keyH', $event.target.value)"></label>
                <label title="Ширина клавиши наибольшая: самый длинный ряд всех страниц сужает её, чтобы влезть в дисплей">key width, mm <input type="number" step="0.1" class="form-control form-control-sm" :value="mm('unitW')" @change="setMm('unitW', $event.target.value)"></label>
                <label title="Промежуток между клавишами">gap, mm <input type="number" step="0.1" class="form-control form-control-sm" :value="mm('gap')" @change="setMm('gap', $event.target.value)"></label>
                <label title="Поле между клавишами и краями полосы">margin, mm <input type="number" step="0.1" class="form-control form-control-sm" :value="mm('margin')" @change="setMm('margin', $event.target.value)"></label>
                <label title="Скругление углов клавиши">radius, mm <input type="number" step="0.1" class="form-control form-control-sm" :value="mm('radius')" @change="setMm('radius', $event.target.value)"></label>
            </div>
        </section>
        <section>
            <h6 title="Цвета: фон полосы — переход сверху вниз; на дисплее они — RGB565, справа видно, какими станут">Theme</h6>
            <div class="kb-presets"><button v-for="(t, name) in presets" :key="name" class="btn btn-outline-secondary btn-sm" :title="'Взять тему ' + name" @click="setTheme(name)"><span class="kb-swatch" :style="{ background: 'linear-gradient(' + t.backTop + ',' + t.backBottom + ')' }"></span>{{ name }}</button></div>
            <div v-for="(text, name) in colors" :key="name" class="kb-color">
                <input type="color" :value="keyboardStore.layout.theme[name]" @change="setColor(name, $event.target.value)">
                <span>{{ text }}</span>
                <span class="kb-swatch" :style="{ background: shown(name) }" :title="'На дисплее: ' + shown(name)"></span>
            </div>
        </section>
        <section v-if="key">
            <h6 title="Клавиша, выбранная щелчком на холсте">Key <span class="kb-note">row {{ keyboardStore.edit.row + 1 }}, {{ keyboardStore.edit.key + 1 }} · {{ codeText(key) }}<span v-if="laid"> · {{ laid.w }}×{{ laid.h }} px</span></span></h6>
            <select class="form-select form-select-sm mb-1" :value="kind" @change="setKind($event.target.value)" title="Что делает клавиша">
                <option v-for="(text, k) in kinds" :key="k" :value="k">{{ text }}</option>
            </select>
            <div class="kb-grid2">
                <label v-if="kind === 'char'" title="Символ, который она набирает">types <input class="form-control form-control-sm" :value="key.char" @change="setChar($event.target.value)"></label>
                <label v-if="kind === 'char'" title="Что с Shift; пусто — то же">with shift <input class="form-control form-control-sm" :value="key.upper || ''" @change="setUpper($event.target.value)"></label>
                <label v-if="kind === 'page'" title="На какую страницу ведёт">to page <select class="form-select form-select-sm" :value="key.page" @change="setPage($event.target.value)"><option v-for="(p, i) in keyboardStore.layout.pages" :key="i" :value="i">{{ i + 1 }}. {{ p.name }}</option></select></label>
                <label title="Надпись на ней вместо символа; у стереть, готово, отмена, Shift без надписи рисуется знак">label <input class="form-control form-control-sm" :value="key.label || ''" @change="setLabel($event.target.value)"></label>
                <label title="Ширина в четвертях клавиши: 4 — одна, 6 — полторы, 20 — пробел в пять">width, ¼ <input type="number" min="1" max="255" class="form-control form-control-sm" :value="key.width || 4" @change="setWidth($event.target.value)"></label>
            </div>
            <div class="btn-group btn-group-sm mt-1">
                <button class="btn btn-outline-secondary" title="Сдвинуть влево" @click="keyboardStore.moveKey(-1)">←</button>
                <button class="btn btn-outline-secondary" title="Сдвинуть вправо" @click="keyboardStore.moveKey(1)">→</button>
                <button class="btn btn-outline-secondary" title="Новая клавиша перед ней" @click="keyboardStore.addKey(false)">+ before</button>
                <button class="btn btn-outline-secondary" title="Новая клавиша после неё" @click="keyboardStore.addKey(true)">+ after</button>
                <button class="btn btn-outline-danger" title="Убрать клавишу" @click="keyboardStore.removeKey()">✕</button>
            </div>
            <div class="btn-group btn-group-sm mt-1">
                <span class="kb-note me-1">row</span>
                <button class="btn btn-outline-secondary" title="Ряд выше" @click="keyboardStore.moveRow(-1)">↑</button>
                <button class="btn btn-outline-secondary" title="Ряд ниже" @click="keyboardStore.moveRow(1)">↓</button>
                <button class="btn btn-outline-secondary" title="Новый ряд над этим" @click="keyboardStore.addRow(false)">+ above</button>
                <button class="btn btn-outline-secondary" title="Новый ряд под этим" @click="keyboardStore.addRow(true)">+ below</button>
                <button class="btn btn-outline-danger" title="Убрать ряд" @click="keyboardStore.removeRow()">✕</button>
            </div>
        </section>
        <section>
            <h6 title="Страница, показанная на холсте">Page {{ keyboardStore.edit.page + 1 }} of {{ keyboardStore.layout.pages.length }}</h6>
            <label class="kb-row">name <input class="form-control form-control-sm" :value="pageName" @change="pageName = $event.target.value"></label>
            <div class="btn-group btn-group-sm mt-1">
                <button class="btn btn-outline-secondary" title="Новая страница после этой: на ней клавиша назад" @click="keyboardStore.addPage()">+ page</button>
                <button class="btn btn-outline-danger" title="Убрать страницу; клавиши, что вели на неё, ведут на первую" @click="keyboardStore.removePage()">✕ page</button>
            </div>
        </section>
        <section v-if="keyboardStore.problems.length">
            <h6 title="Ошибки — DGX-DI такую раскладку не откроет; предупреждения — откроет, но, может быть, не так, как задумано">Problems</h6>
            <div v-for="(p, i) in keyboardStore.problems" :key="i" :class="p.level === 'error' ? 'kb-error' : 'kb-warning'" class="kb-problem">{{ p.text }}</div>
        </section>
    </div>`
};
