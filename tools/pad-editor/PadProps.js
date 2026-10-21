// The properties on the right while editing: the display and what lies under the pad, the layer's alpha; the look of
// every item; the items; the chosen one's kind, button, place, size, color and look; what is wrong with the layout.
const PadProps = {
    data() {
        return {
            themes: Object.keys(PadLayout.Themes), icons: PadIcons, groups: PadLayout.BitGroups, palette: PadLayout.Palette, kinds: PadLayout.Kinds,
            // a cross, a button and a stick, to show each look on
            lookItems: [{ kind: 'cross', r: 130 }, { kind: 'button', r: 95, out: 'A', color: '#29cf29' }],
            tips: {
                Default: 'Без рисунков: элементы рисует сама DGX-DI (круг с ободком и знаком, крестовина с шевронами, лунка со шляпкой). Так выглядит и классическая раскладка',
                Flat: 'Плоские заливки без ободков', Glass: 'Стекло: тёмная кромка и блик', Outline: 'Контуры: только обводка, нажатая — залита', Neon: 'Неон: чёрное с светящимся кольцом'
            },
            fromNames: { start: ['from the left', 'from the top'], middle: ['from the middle', 'from the middle'], end: ['from the right', 'from the bottom'] }
        };
    },
    computed: {
        ...Pinia.mapStores(usePadStore),
        displays() {
            return PadLayout.Displays;
        },
        display() {
            return this.padStore.edit.display;
        },
        presetName: {
            get() {
                const d = this.display, p = PadLayout.Displays.find(p => p.width === d.width && p.height === d.height && p.widthMm === d.widthMm && p.heightMm === d.heightMm);
                return p ? p.name : '';
            },
            set(name) {
                const p = PadLayout.Displays.find(p => p.name === name);
                if (p) this.padStore.setDisplay(p);
            }
        },
        density() {
            const d = this.display;
            return `${(d.width / d.widthMm).toFixed(2)} px a mm · ${(Math.hypot(d.widthMm, d.heightMm) / 25.4).toFixed(1)}″`;
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
        pictureNames() {
            return this.item ? PadLayout.Pictures[this.item.kind] : [];
        },
        // the look the chosen item has: a theme's pictures, none (Default), or its own
        look() {
            const it = this.item;
            if (!it.pictures) return 'Default';
            const own = JSON.stringify(it.pictures);
            return this.themes.find(t => t !== 'Default' && JSON.stringify(PadLayout.Themes[t](it)) === own) || '';
        },
        drawn() {
            const p = this.item.pictures || {};
            return this.pictureNames.filter(n => p[n] && p[n].length).length;
        },
        across() {
            return (this.item.r / 5).toFixed(1);
        },
        // the problems of the chosen item, to tell beside it
        itemProblems() {
            return this.padStore.problems.filter(p => p.item === this.padStore.edit.item);
        }
    },
    methods: {
        setDisplayField(name, value) {
            const v = parseInt(value, 10);
            if (v > 0 && v <= 8192) this.padStore.setDisplay({ ...this.display, name: 'custom', [name]: v });
        },
        background(b) {
            this.padStore.edit.background = b;
            this.padStore.save();
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
            const it = this.item;
            if (kind === it.kind) return;
            const out = kind === 'button' ? (PadLayout.Bits.includes(it.out) ? it.out : 'A') : kind === 'stick' ? (it.out === 'right' ? 'right' : 'left') : undefined;
            this.padStore.setItem({ kind, out, four: kind === 'cross' ? it.four : undefined, floating: kind === 'stick' ? it.floating : undefined, pictures: undefined });
        },
        loadImage(event) {
            const file = event.target.files[0];
            event.target.value = '';
            if (!file) return;
            const img = new Image();
            img.onload = () => {
                this.padStore.image = img, this.padStore.edit.background = 'image';
                this.padStore.say(`${file.name} under the pad`, 'ok');
            };
            img.src = URL.createObjectURL(file);
        },
        name(it) {
            return PadLayout.nameOf(it);
        },
        draw() {
            this.padStore.editPicture(this.pictureNames[0]);
        },
        label(g, b) {
            return g.labels ? g.labels[b] : b;
        }
    },
    template: `
    <div class="pd-props">
        <section>
            <h6 title="Дисплей, на котором видна раскладка: его пиксели и миллиметры, как он стоит в программе. Раскладка в миллиметрах, на другом дисплее она ляжет так же">Display</h6>
            <select class="form-select form-select-sm mb-1" v-model="presetName" title="Платы, на которых DGX-DI проверяли; свой дисплей — впиши пиксели и миллиметры">
                <option value="" disabled>custom…</option>
                <option v-for="d in displays" :key="d.name" :value="d.name">{{ d.name }}</option>
            </select>
            <div class="pd-dims" title="Пиксели дисплея, как он стоит в программе (ширина × высота)"><span>px</span><input type="number" class="form-control form-control-sm" :value="display.width" @change="setDisplayField('width', $event.target.value)"><span>×</span><input type="number" class="form-control form-control-sm" :value="display.height" @change="setDisplayField('height', $event.target.value)"></div>
            <div class="pd-dims" title="Миллиметры видимой области дисплея (ширина × высота)"><span>mm</span><input type="number" class="form-control form-control-sm" :value="display.widthMm" @change="setDisplayField('widthMm', $event.target.value)"><span>×</span><input type="number" class="form-control form-control-sm" :value="display.heightMm" @change="setDisplayField('heightMm', $event.target.value)"></div>
            <div class="pd-note mb-1">{{ density }}</div>
            <div class="pd-field" title="Что под геймпадом: придуманная картинка игры, тёмное, светлое или своя картинка (кадр своей игры)"><span>Under it</span>
                <div class="btn-group btn-group-sm">
                    <button v-for="b in [['scene', 'Game'], ['dark', 'Dark'], ['light', 'Light']]" :key="b[0]" class="btn" :class="padStore.edit.background === b[0] ? 'btn-secondary' : 'btn-outline-secondary'" @click="background(b[0])">{{ b[1] }}</button>
                    <button v-if="padStore.image" class="btn" :class="padStore.edit.background === 'image' ? 'btn-secondary' : 'btn-outline-secondary'" @click="background('image')">Image</button>
                    <label class="btn btn-outline-secondary mb-0" title="Своя картинка под геймпадом (PNG, JPEG): например, кадр своей игры"><span v-html="icons.image"></span><input type="file" accept="image/*" hidden @change="loadImage"></label>
                </div>
            </div>
            <div class="pd-field" :title="'Насколько слой геймпада виден поверх картинки — alpha, которую даёшь dgx_di_pad_show(): 0..255. DGX смешивает по ' + weight + ' из 32'"><span>Layer alpha</span>
                <div><input type="range" min="0" max="255" class="form-range" v-model.number="padStore.edit.alpha" @change="padStore.save()"><b class="pd-value">{{ padStore.edit.alpha }}</b></div>
            </div>
        </section>
        <section>
            <h6 title="Вид всех элементов сразу: тема даёт каждому рисунки по его виду, размеру и цвету. Потом любой элемент можно перерисовать отдельно">Look of every item</h6>
            <div class="pd-looks">
                <button v-for="t in themes" :key="t" class="pd-look" :title="tips[t]" @click="padStore.theme(t, true)"><ItemThumb :items="lookItems" :theme="t" :height="30"></ItemThumb><span>{{ t }}</span></button>
            </div>
        </section>
        <section>
            <h6 title="Элементы раскладки. Где два перекрываются, палец берёт тот, что выше в списке">Items
                <span class="ms-auto btn-group btn-group-sm">
                    <button class="btn btn-outline-primary" title="Новая кнопка посередине дисплея" @click="padStore.addItem('button')"><span v-html="icons.button"></span> button</button>
                    <button class="btn btn-outline-primary" title="Новая крестовина посередине" @click="padStore.addItem('cross')"><span v-html="icons.cross"></span> cross</button>
                    <button class="btn btn-outline-primary" title="Новый стик посередине" @click="padStore.addItem('stick')"><span v-html="icons.stick"></span> stick</button>
                </span>
            </h6>
            <div class="pd-items">
                <div v-for="(it, k) in padStore.layout.items" :key="k" class="pd-item" :class="{ chosen: k === padStore.edit.item }" @click="padStore.choose(k)" @dblclick="padStore.choose(k); draw()" title="Щелчок — выбрать; двойной — рисовать его рисунки">
                    <span class="pd-kind" v-html="icons[it.kind]"></span><span class="pd-swatch" :style="{ background: it.color || '#5a5a5a' }"></span><span class="pd-item-name">{{ name(it) }}</span>
                    <span v-if="it.pictures" class="pd-note" title="У него свои рисунки" v-html="icons.pencil"></span>
                    <span v-if="padStore.problems.some(p => p.item === k)" class="pd-note pd-warning" title="С ним что-то не так: смотри ниже" v-html="icons.warning"></span>
                </div>
            </div>
        </section>
        <section v-if="item" class="pd-item-section">
            <h6><span class="pd-kind" v-html="icons[item.kind]"></span>{{ name(item) }} <span class="pd-note" v-if="place" title="Где он на этом дисплее, в пикселях: середина и радиус">{{ place.x }}, {{ place.y }} · r {{ place.r }} px</span>
                <span class="ms-auto btn-group btn-group-sm pd-item-tools">
                    <button class="btn btn-outline-secondary" title="Выше в списке: при наложении палец берёт его первым" :disabled="padStore.edit.item === 0" @click="padStore.orderItem(-1)" v-html="icons.up"></button>
                    <button class="btn btn-outline-secondary" title="Ниже в списке" :disabled="padStore.edit.item === padStore.layout.items.length - 1" @click="padStore.orderItem(1)" v-html="icons.down"></button>
                    <button class="btn btn-outline-secondary" title="Копия рядом (Ctrl+D)" @click="padStore.duplicateItem()" v-html="icons.copy"></button>
                    <button class="btn btn-outline-danger" title="Убрать (Delete)" @click="padStore.removeItem()" v-html="icons.trash"></button>
                </span>
            </h6>
            <div class="pd-field"><span>Kind</span>
                <div class="btn-group btn-group-sm">
                    <button v-for="k in kinds" :key="k" class="btn" :class="item.kind === k ? 'btn-secondary' : 'btn-outline-secondary'" :title="{ button: 'Кнопка: один бит геймпада', cross: 'Крестовина: четыре стороны и диагонали', stick: 'Стик: две оси' }[k]" @click="setKind(k)"><span v-html="icons[k]"></span> {{ k }}</button>
                </div>
            </div>
            <div v-if="item.kind === 'button'" class="pd-field pd-field-top" title="Какую кнопку геймпада она нажимает (бит dgx_di_pad_t, как в dgx_ge_input.h)"><span>Button</span>
                <div class="pd-bit-groups">
                    <div v-for="g in groups" :key="g.name" class="pd-bits">
                        <button v-for="b in g.bits" :key="b" class="pd-bit" :class="{ chosen: item.out === b }" @click="padStore.setItem({ out: b })">{{ label(g, b) }}</button>
                    </div>
                </div>
            </div>
            <div v-if="item.kind === 'stick'" class="pd-field"><span>Stick</span>
                <div class="btn-group btn-group-sm" title="Левый или правый стик: чьи оси он даёт">
                    <button class="btn" :class="(item.out || 'left') === 'left' ? 'btn-secondary' : 'btn-outline-secondary'" @click="padStore.setItem({ out: 'left' })">left</button>
                    <button class="btn" :class="item.out === 'right' ? 'btn-secondary' : 'btn-outline-secondary'" @click="padStore.setItem({ out: 'right' })">right</button>
                </div>
            </div>
            <label v-if="item.kind === 'stick'" class="pd-check" title="Середина стика там, где опустился палец: удобно, когда не глядишь на экран"><input type="checkbox" :checked="item.floating" @change="padStore.setItem({ floating: $event.target.checked })"> floating: its middle where the finger goes down</label>
            <label v-if="item.kind === 'cross'" class="pd-check" title="Только четыре стороны, без диагоналей: из угла — та сторона, к которой ближе"><input type="checkbox" :checked="item.four" @change="padStore.setItem({ four: $event.target.checked })"> four ways only, no diagonals</label>
            <div class="pd-field pd-field-top"><span>Place</span>
                <div class="pd-place">
                    <AnchorPicker :fromX="item.fromX || 'start'" :fromY="item.fromY || 'start'" @change="padStore.setItem($event)"></AnchorPicker>
                    <div class="pd-place-fields">
                        <label title="Расстояние до середины элемента по x, мм; от середины дисплея — вправо (может быть меньше нуля)"><span>x</span><input type="number" step="0.5" class="form-control form-control-sm" :value="mm('x')" @change="setMm('x', $event.target.value)"><span class="pd-note">mm {{ fromNames[item.fromX || 'start'][0] }}</span></label>
                        <label title="Расстояние до середины элемента по y, мм; от середины дисплея — вниз"><span>y</span><input type="number" step="0.5" class="form-control form-control-sm" :value="mm('y')" @change="setMm('y', $event.target.value)"><span class="pd-note">mm {{ fromNames[item.fromY || 'start'][1] }}</span></label>
                    </div>
                </div>
            </div>
            <div class="pd-field" title="Радиус круга, за который его берёт палец; рисунки растут с ним. Меньше 8 мм поперёк — промахи"><span>Size</span>
                <label class="pd-inline"><span>r</span><input type="number" step="0.5" min="1" class="form-control form-control-sm" :value="mm('r')" @change="setMm('r', $event.target.value)"><span class="pd-note">mm · {{ across }} mm across</span></label>
            </div>
            <div class="pd-field" title="Цвет элемента: фигуры цвета item, light, dark берут его"><span>Color</span>
                <div class="pd-swatches">
                    <button v-for="c in palette" :key="c" class="pd-swatch-button" :class="{ chosen: (item.color || '#5a5a5a') === c }" :style="{ background: c }" :title="c" @click="padStore.setItem({ color: c === '#5a5a5a' ? undefined : c })"></button>
                    <input type="color" class="form-control form-control-sm form-control-color" title="Свой цвет" :value="item.color || '#5a5a5a'" @change="padStore.setItem({ color: $event.target.value })">
                </div>
            </div>
            <div class="pd-field pd-field-top" title="Вид этого элемента: тема или свои рисунки"><span>Look</span>
                <div>
                    <div class="pd-looks pd-looks-one">
                        <button v-for="t in themes" :key="t" class="pd-look" :class="{ chosen: look === t }" :title="tips[t]" @click="padStore.theme(t, false)"><ItemThumb :items="[item]" :theme="t" :height="36"></ItemThumb><span>{{ t }}</span></button>
                    </div>
                    <div class="pd-look-tools">
                        <button class="btn btn-outline-primary btn-sm" title="Нарисовать его самому: фигуры в миллиметрах от середины, каждому состоянию свой рисунок" @click="draw()"><span v-html="icons.pencil"></span> Draw its pictures…</button>
                        <span class="pd-note" v-if="look === ''">own pictures: {{ drawn }} of {{ pictureNames.length }} drawn</span>
                        <button v-if="item.pictures" class="btn btn-outline-secondary btn-sm" :title="'Эти рисунки — всем элементам того же вида, с поправкой на размер (цвет item у каждого свой)'" @click="padStore.picturesToKind()">to every {{ item.kind }}</button>
                    </div>
                </div>
            </div>
            <div v-for="(p, i) in itemProblems" :key="i" class="pd-problem" :class="p.level === 'error' ? 'pd-error' : 'pd-warning'"><span v-html="icons[p.level]"></span> {{ p.text }}</div>
        </section>
        <section v-if="padStore.problems.length">
            <h6 title="Ошибки — такую раскладку программа не откроет; предупреждения — откроет, но, может быть, не так, как задумано. Щелчок — выбрать элемент">Problems</h6>
            <div v-for="(p, i) in padStore.problems" :key="i" :class="p.level === 'error' ? 'pd-error' : 'pd-warning'" class="pd-problem pd-problem-link" @click="p.item !== undefined && padStore.choose(p.item)"><span v-html="icons[p.level]"></span> {{ p.text }}</div>
        </section>
    </div>`
};
