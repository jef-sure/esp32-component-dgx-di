// The workbench: the tools along the top (a new layout of a template, a file opened and saved, undo, redo, editing or trying, the
// zoom), the pages above the display, the properties on the right, the line of state at the bottom. A JSON file
// dropped onto it is opened.
const KeyboardEdit = {
    data() {
        return { downloadUrl: undefined, dragging: false, menu: false };
    },
    computed: {
        ...Pinia.mapStores(useKeyboardStore),
        templates: () => KeyboardLayout.Templates,
        fileName() {
            return (this.keyboardStore.layout.name || 'keyboard').replace(/[^\w.-]+/g, '_') + '.json';
        },
        status() {
            const s = this.keyboardStore, b = s.band, d = s.edit.display;
            if (!b) return 'the keys do not fit the display';
            const mm = (px, along, of) => (px * of / along).toFixed(1);
            const errors = s.problems.filter(p => p.level === 'error').length, warnings = s.problems.length - errors;
            return `band ${b.w}×${b.h} at ${b.x}, ${b.y} · a key ${b.unit}×${b.keyH} px (${mm(b.unit, d.width, d.widthMm)}×${mm(b.keyH, d.height, d.heightMm)} mm) · ` +
                `page ${s.pageIndex + 1}: ${s.keys.length} keys` + (errors ? ` · ${errors} errors` : '') + (warnings ? ` · ${warnings} warnings` : '');
        },
        trialText() {
            const t = this.keyboardStore.trial;
            return t.flash === 'done' ? `done: "${t.text}"` : t.flash === 'cancel' ? 'cancelled' : t.shift ? 'shift' : '';
        }
    },
    mounted() {
        window.addEventListener('keydown', this.keys);
        window.addEventListener('mousedown', this.away);
    },
    beforeUnmount() {
        window.removeEventListener('keydown', this.keys);
        window.removeEventListener('mousedown', this.away);
    },
    methods: {
        keys(event) {
            if (event.key === 'Escape') this.menu = false;
            if (event.target.tagName === 'INPUT' || event.target.tagName === 'SELECT') return;
            const z = event.key.toLowerCase() === 'z' && (event.ctrlKey || event.metaKey);
            if (z && !event.shiftKey) this.keyboardStore.undo(), event.preventDefault();
            else if ((z && event.shiftKey) || (event.key.toLowerCase() === 'y' && event.ctrlKey)) this.keyboardStore.redo(), event.preventDefault();
        },
        // the menu of templates: closed by a click elsewhere or by Escape
        away(event) {
            if (this.menu && !this.$refs.templates.contains(event.target)) this.menu = false;
        },
        begin(name) {
            this.menu = false;
            this.keyboardStore.newLayout(name);
        },
        save() {
            const blob = new Blob([KeyboardLayout.toJson(this.keyboardStore.layout)], { type: 'application/json' });
            if (this.downloadUrl) URL.revokeObjectURL(this.downloadUrl);
            this.downloadUrl = URL.createObjectURL(blob);
            this.$nextTick(() => this.$refs.download.click());
            this.keyboardStore.message = `saved as ${this.fileName}: python3 tools/keyboard_blob.py ${this.fileName} out.kbd makes the blob`;
        },
        // a layout of JSON; a font (a file of TrueType or OpenType, a font of DGX: its C or the JSON of dgx_ge_font.py)
        read(file) {
            if (!file) return;
            if (/\.(ttf|otf|ttc|woff2?|c)$/i.test(file.name) || /\.json$/i.test(file.name)) return this.keyboardStore.openFontFile(file);
            const reader = new FileReader();
            reader.onload = () => this.keyboardStore.open(reader.result, file.name);
            reader.readAsText(file);
        },
        chosen(event) {
            this.read(event.target.files[0]);
            event.target.value = '';
        },
        dropped(event) {
            this.dragging = false;
            this.read(event.dataTransfer.files[0]);
        },
        mode(m) {
            const s = this.keyboardStore;
            s.edit.mode = m;
            s.trial = { text: '', shift: false, page: s.edit.page, flash: '', pressed: -1 };
            s.save();
        },
        showPage(p) {
            const s = this.keyboardStore;
            if (s.edit.mode === 'try') s.trial.page = p, s.trial.shift = false;
            else s.showPage(p);
        }
    },
    template: `
    <div class="kb-bench" @dragover.prevent="dragging = true" @dragleave="dragging = false" @drop.prevent="dropped" :class="{ 'kb-dragging': dragging }">
        <div class="kb-top">
            <strong class="me-2">DGX-DI keyboard</strong>
            <div class="dropdown" ref="templates">
                <button class="btn btn-outline-secondary btn-sm dropdown-toggle" title="Новая раскладка из заготовки: с неё и начать" @click="menu = !menu">New</button>
                <ul class="dropdown-menu kb-templates" :class="{ show: menu }">
                    <template v-for="(t, i) in templates" :key="t.name">
                        <li v-if="i && !t.own && templates[i - 1].own"><hr class="dropdown-divider"></li>
                        <li><a class="dropdown-item" href="#" :title="t.tip" @click.prevent="begin(t.name)"><strong>{{ t.label }}</strong> <span class="kb-about">{{ t.about }}</span><span v-if="t.own" class="badge bg-light text-secondary ms-2">built in</span></a></li>
                    </template>
                </ul>
            </div>
            <label class="btn btn-outline-secondary btn-sm mb-0" title="Открыть раскладку JSON (или бросить файл на окно); готовые — в examples">Open…<input type="file" accept=".json,application/json" hidden @change="chosen"></label>
            <button class="btn btn-secondary btn-sm" title="Сохранить раскладку файлом JSON (скачать); блоб для программы из него делает tools/keyboard_blob.py" @click="save">Save JSON ⭳</button>
            <a style="display: none" :download="fileName" :href="downloadUrl" ref="download"></a>
            <span class="kb-sep"></span>
            <button class="btn btn-outline-secondary btn-sm" title="Отменить (Ctrl+Z)" :disabled="!keyboardStore.history.length" @click="keyboardStore.undo()">↶</button>
            <button class="btn btn-outline-secondary btn-sm" title="Вернуть (Ctrl+Shift+Z)" :disabled="!keyboardStore.future.length" @click="keyboardStore.redo()">↷</button>
            <span class="kb-sep"></span>
            <div class="btn-group btn-group-sm">
                <button class="btn" :class="keyboardStore.edit.mode === 'edit' ? 'btn-primary' : 'btn-outline-primary'" title="Правка: щелчок выбирает клавишу" @click="mode('edit')">Edit</button>
                <button class="btn" :class="keyboardStore.edit.mode === 'try' ? 'btn-primary' : 'btn-outline-primary'" title="Проба: клавиши печатают в поле, как в DGX-DI" @click="mode('try')">Try</button>
            </div>
            <span class="kb-sep"></span>
            <select class="form-select form-select-sm kb-zoom" v-model.number="keyboardStore.edit.zoom" @change="keyboardStore.save()" title="Масштаб холста: по окну или пикселей экрана на пиксель дисплея">
                <option :value="0">fit</option><option :value="0.5">½</option><option :value="1">1:1</option><option :value="2">2:1</option>
            </select>
        </div>
        <div class="kb-main">
            <ul class="nav nav-tabs kb-pages">
                <li class="nav-item" v-for="(p, i) in keyboardStore.layout.pages" :key="i">
                    <a class="nav-link" :class="{ active: i === keyboardStore.pageIndex }" href="#" @click.prevent="showPage(i)">{{ i + 1 }}. {{ p.name || 'page' }}</a>
                </li>
                <li class="ms-auto kb-trial" v-if="keyboardStore.edit.mode === 'try'">{{ trialText }}</li>
            </ul>
            <KeyboardCanvas></KeyboardCanvas>
        </div>
        <KeyboardProps class="kb-side"></KeyboardProps>
        <div class="kb-status"><span>{{ status }}</span><span class="ms-auto">{{ keyboardStore.message }}</span></div>
    </div>`
};
