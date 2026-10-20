// The workbench: the tools along the top (a layout of a template to begin with, a file opened and saved, undo, redo,
// editing or trying, the zoom), the display in the middle, the picture of the chosen item under it when one is edited,
// the properties on the right, the line of state at the bottom: where the chosen item is, and what a finger gives. A JSON
// file dropped onto it is opened.
const PadEdit = {
    data() {
        return { downloadUrl: undefined, dragging: false, menu: false };
    },
    computed: {
        ...Pinia.mapStores(usePadStore),
        templates: () => PadLayout.Templates,
        fileName() {
            return (this.padStore.layout.name || 'pad').replace(/[^\w.-]+/g, '_') + '.json';
        },
        status() {
            const s = this.padStore, d = s.edit.display, e = s.problems.filter(p => p.level === 'error').length, w = s.problems.length - e;
            const per = `${(d.width / d.widthMm).toFixed(2)} px a mm`;
            let t = `${s.layout.items.length} items · ${per}`;
            if (s.edit.mode === 'try') {
                const names = PadLayout.Bits.filter(b => s.trial.buttons & PadLayout.bit(b));
                t += ` · held: ${names.length ? names.join(' ') : 'none'}`;
                if (s.trial.axes) t += ` · ${s.trial.axes.side} stick ${s.trial.axes.x}, ${s.trial.axes.y}`;
            }
            return t + (e ? ` · ${e} errors` : '') + (w ? ` · ${w} warnings` : '');
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
            if (z && !event.shiftKey) this.padStore.undo(), event.preventDefault();
            else if ((z && event.shiftKey) || (event.key.toLowerCase() === 'y' && event.ctrlKey)) this.padStore.redo(), event.preventDefault();
        },
        // the menu of templates: closed by a click elsewhere or by Escape
        away(event) {
            if (this.menu && !this.$refs.templates.contains(event.target)) this.menu = false;
        },
        begin(name) {
            this.menu = false;
            this.padStore.newLayout(name);
        },
        save() {
            const blob = new Blob([PadLayout.toJson(this.padStore.layout)], { type: 'application/json' });
            if (this.downloadUrl) URL.revokeObjectURL(this.downloadUrl);
            this.downloadUrl = URL.createObjectURL(blob);
            this.$nextTick(() => this.$refs.download.click());
            this.padStore.message = `saved as ${this.fileName}: python3 tools/pad_blob.py ${this.fileName} out.pad makes the blob`;
        },
        read(file) {
            if (!file) return;
            const reader = new FileReader();
            reader.onload = () => this.padStore.open(reader.result, file.name);
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
            const s = this.padStore;
            s.edit.mode = m, s.edit.picture = '';
            s.fingerUp();
            s.save();
        }
    },
    template: `
    <div class="pd-bench" :class="{ 'pd-with-picture': padStore.edit.picture && padStore.edit.mode === 'edit', 'pd-dragging': dragging }" @dragover.prevent="dragging = true" @dragleave="dragging = false" @drop.prevent="dropped">
        <div class="pd-top">
            <strong class="me-2">DGX-DI pad</strong>
            <div class="dropdown" ref="templates">
                <button class="btn btn-outline-secondary btn-sm dropdown-toggle" title="Новая раскладка из заготовки: с неё и начать" @click="menu = !menu">New</button>
                <ul class="dropdown-menu pd-templates" :class="{ show: menu }">
                    <template v-for="(t, i) in templates" :key="t.name">
                        <li v-if="i && !t.own && templates[i - 1].own"><hr class="dropdown-divider"></li>
                        <li><a class="dropdown-item" href="#" :title="t.tip" @click.prevent="begin(t.name)"><strong>{{ t.label }}</strong> <span class="pd-about">{{ t.about }}</span><span v-if="t.own" class="badge bg-light text-secondary ms-2">built in</span></a></li>
                    </template>
                </ul>
            </div>
            <label class="btn btn-outline-secondary btn-sm mb-0" title="Открыть раскладку JSON (или бросить файл на окно); готовые — в examples">Open…<input type="file" accept=".json,application/json" hidden @change="chosen"></label>
            <button class="btn btn-secondary btn-sm" title="Сохранить раскладку файлом JSON (скачать); блоб для программы из него делает tools/pad_blob.py" @click="save">Save JSON ⭳</button>
            <a style="display: none" :download="fileName" :href="downloadUrl" ref="download"></a>
            <span class="pd-sep"></span>
            <button class="btn btn-outline-secondary btn-sm" title="Отменить (Ctrl+Z)" :disabled="!padStore.history.length" @click="padStore.undo()">↶</button>
            <button class="btn btn-outline-secondary btn-sm" title="Вернуть (Ctrl+Shift+Z)" :disabled="!padStore.future.length" @click="padStore.redo()">↷</button>
            <span class="pd-sep"></span>
            <div class="btn-group btn-group-sm">
                <button class="btn" :class="padStore.edit.mode === 'edit' ? 'btn-primary' : 'btn-outline-primary'" title="Правка: элементы двигать и менять" @click="mode('edit')">Edit</button>
                <button class="btn" :class="padStore.edit.mode === 'try' ? 'btn-primary' : 'btn-outline-primary'" title="Проба: мышь — палец, как DGX-DI возьмёт его" @click="mode('try')">Try</button>
            </div>
            <span class="pd-sep"></span>
            <select class="form-select form-select-sm pd-zoom" v-model.number="padStore.edit.zoom" @change="padStore.save()" title="Масштаб: по окну или пикселей экрана на пиксель дисплея">
                <option :value="0">fit</option><option :value="0.5">½</option><option :value="1">1:1</option><option :value="2">2:1</option>
            </select>
            <label class="pd-row ms-2 mb-0" title="Имя раскладки (в блобе и в имени файла)">name <input class="form-control form-control-sm" :value="padStore.layout.name" @change="padStore.change(l => { l.name = $event.target.value; })"></label>
        </div>
        <div class="pd-main"><PadCanvas></PadCanvas></div>
        <PictureEditor v-if="padStore.edit.picture && padStore.edit.mode === 'edit'" class="pd-strip" :key="padStore.edit.item + padStore.edit.picture"></PictureEditor>
        <PadProps class="pd-side"></PadProps>
        <div class="pd-status"><span>{{ status }}</span><span class="ms-auto">{{ padStore.message }}</span></div>
    </div>`
};
