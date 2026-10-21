// The workbench: the tools along the top (a layout of a template to begin with, a file opened, the layout saved as JSON
// and as the blob the program takes, undo, redo, editing or trying, the zoom, the grid, the name, the help), the display
// in the middle (or the picture of the chosen item, large, while one is drawn), the properties on the right (the readout
// of the pad in Try), the line of state at the bottom. A JSON file dropped onto it is opened.
const PadEdit = {
    data() {
        return { downloadUrl: undefined, dragging: false, menu: '', help: false, icons: PadIcons };
    },
    computed: {
        ...Pinia.mapStores(usePadStore),
        templates: () => PadLayout.Templates,
        own() { return PadLayout.Templates.filter(t => t.own); },
        games() { return PadLayout.Templates.filter(t => !t.own); },
        baseName() {
            return (this.padStore.layout.name || 'pad').replace(/[^\w.-]+/g, '_');
        },
        fileName() {
            return this.baseName + '.json';
        },
        cName() {
            return 'pad_' + PadLayout.ident(this.padStore.layout.name);
        },
        errors() {
            return this.padStore.problems.filter(p => p.level === 'error').length;
        },
        drawing() {
            return this.padStore.edit.picture && this.padStore.edit.mode === 'edit';
        },
        status() {
            const s = this.padStore, d = s.edit.display, e = this.errors, w = s.problems.length - e;
            let t = `${s.layout.items.length} items on ${d.width}×${d.height} px, ${d.widthMm}×${d.heightMm} mm`;
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
            const s = this.padStore, target = event.target, tag = target.tagName;
            if (event.key === 'Escape') {
                if (this.menu) return this.menu = '';
                if (this.help) return this.help = false;
            }
            // the letter of the key: its own when Latin (QWERTZ keeps its z), else by its place (Ctrl+Я is Ctrl+Z)
            const letter = /^[a-z]$/i.test(event.key) ? event.key.toLowerCase() : /^Key[A-Z]$/.test(event.code) ? event.code.slice(3).toLowerCase() : event.key.toLowerCase();
            const ctrl = event.ctrlKey || event.metaKey;
            // in a field of text the browser's own undo applies; from a list, a slider or a button undo is the layout's
            const typing = tag === 'TEXTAREA' || (tag === 'INPUT' && !['checkbox', 'range', 'file', 'color', 'button'].includes(target.type));
            if (!typing) {
                if (letter === 'z' && ctrl && !event.shiftKey) return s.undo(), event.preventDefault();
                if ((letter === 'z' && ctrl && event.shiftKey) || (letter === 'y' && ctrl)) return s.redo(), event.preventDefault();
            }
            if (tag === 'INPUT' || tag === 'SELECT' || tag === 'TEXTAREA') return;
            if (event.key === '?' || (letter === 'h' && !ctrl)) return this.help = !this.help;
            if (this.drawing || s.edit.mode !== 'edit') return; // the picture has its own keys
            if (letter === 'd' && ctrl) return s.duplicateItem(), event.preventDefault();
            if (event.key === 'Delete' || event.key === 'Backspace') return s.removeItem(), event.preventDefault();
            const way = { ArrowLeft: [-1, 0], ArrowRight: [1, 0], ArrowUp: [0, -1], ArrowDown: [0, 1] }[event.key];
            if (way) {
                const step = event.altKey ? 1 : event.shiftKey ? 50 : 10; // a millimetre; Shift 5; Alt a tenth
                s.nudgeItem(way[0] * step, way[1] * step);
                event.preventDefault();
            }
        },
        // the menus: closed by a click elsewhere or by Escape
        away(event) {
            if (this.menu && !this.$refs[this.menu].contains(event.target)) this.menu = '';
        },
        toggle(name) {
            this.menu = this.menu === name ? '' : name;
        },
        begin(name) {
            this.menu = '';
            this.padStore.newLayout(name);
        },
        download(blob, name) {
            if (this.downloadUrl) URL.revokeObjectURL(this.downloadUrl);
            this.downloadUrl = URL.createObjectURL(blob);
            this.$refs.download.download = name;
            this.$nextTick(() => this.$refs.download.click());
        },
        saveJson() {
            this.download(new Blob([PadLayout.toJson(this.padStore.layout)], { type: 'application/json' }), this.fileName);
            this.padStore.setOrigin(`the saved ${this.fileName}`);
            this.padStore.say(`saved as ${this.fileName}: open it here again, or make the blob of it: python3 tools/pad_blob.py ${this.fileName} ${this.baseName}.pad`, 'ok');
        },
        // the blob: refused with what is wrong, as the program would refuse it
        blob() {
            try {
                return PadLayout.toBlob(this.padStore.layout);
            } catch (e) {
                this.padStore.say(`not made: ${e.message}`, 'error');
                return null;
            }
        },
        savePad() {
            this.menu = '';
            const data = this.blob();
            if (!data) return;
            this.download(new Blob([data], { type: 'application/octet-stream' }), this.baseName + '.pad');
            this.padStore.say(`saved as ${this.baseName}.pad (${data.length} bytes): embed it (EMBED_FILES) and open it: dgx_di_pad_open(&pad, screen, w_mm, h_mm, ${this.baseName}_start, ${this.baseName}_end - ${this.baseName}_start)`, 'ok');
        },
        saveC() {
            this.menu = '';
            const data = this.blob();
            if (!data) return;
            this.download(new Blob([PadLayout.toC(this.cName, data, this.fileName)], { type: 'text/x-c' }), this.baseName + '.c');
            this.padStore.say(`saved as ${this.baseName}.c (${data.length} bytes): add it to the program and open it: dgx_di_pad_open(&pad, screen, w_mm, h_mm, ${this.cName}, ${this.cName}_size)`, 'ok');
        },
        async copyC() {
            this.menu = '';
            const data = this.blob();
            if (!data) return;
            try {
                await navigator.clipboard.writeText(PadLayout.toC(this.cName, data, this.fileName));
                this.padStore.say(`the C array ${this.cName} (${data.length} bytes) is in the clipboard: paste it into the program and open it with dgx_di_pad_open(&pad, screen, w_mm, h_mm, ${this.cName}, ${this.cName}_size)`, 'ok');
            } catch (e) {
                this.padStore.say(`not copied: ${e.message}`, 'error');
            }
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
            if (m === 'try') s.say('Try: the mouse is a finger. Press the buttons, lead it over the cross and the stick; what the program gets is on the right', '');
            else s.say('', '');
        }
    },
    template: `
    <div class="pd-bench" :class="{ 'pd-dragging': dragging }" @dragover.prevent="dragging = true" @dragleave="dragging = false" @drop.prevent="dropped">
        <div class="pd-top">
            <span class="pd-brand" title="Редактор раскладок геймпада DGX-DI: где на дисплее кнопки, крестовины и стики и как они выглядят">DGX-DI <b>pad</b></span>
            <div class="pd-menu-box" ref="new">
                <button class="btn btn-primary btn-sm" title="Новая раскладка из заготовки: с неё и начать" @click="toggle('new')"><span v-html="icons.plus"></span> New ▾</button>
                <div class="pd-menu pd-templates" :class="{ show: menu === 'new' }">
                    <div class="pd-menu-head">Начать с заготовки — потом подвинуть, перекрасить, добавить своё. Заготовки нарисованы на выбранном дисплее.</div>
                    <div class="pd-menu-group">DGX-DI's own, built into it</div>
                    <div class="pd-cards">
                        <a v-for="t in own" :key="t.name" class="dropdown-item pd-card" href="#" :title="t.tip" @click.prevent="begin(t.name)"><PadThumb :layout="t.make()" :display="padStore.edit.display" :width="150"></PadThumb><strong>{{ t.label }}</strong><span class="pd-about">{{ t.about }}</span></a>
                    </div>
                    <div class="pd-menu-group">The pads of the usual games</div>
                    <div class="pd-cards">
                        <a v-for="t in games" :key="t.name" class="dropdown-item pd-card" href="#" :title="t.tip" @click.prevent="begin(t.name)"><PadThumb :layout="t.make()" :display="padStore.edit.display" :width="150"></PadThumb><strong>{{ t.label }}</strong><span class="pd-about">{{ t.about }}</span></a>
                    </div>
                </div>
            </div>
            <label class="btn btn-outline-secondary btn-sm mb-0" title="Открыть раскладку JSON, сохранённую здесь (или бросить файл на окно); готовые — в examples"><span v-html="icons.folder"></span> Open…<input type="file" accept=".json,application/json" hidden @change="chosen"></label>
            <button class="btn btn-outline-secondary btn-sm" title="Сохранить раскладку файлом JSON — чтобы открыть здесь снова (скачать)" @click="saveJson"><span v-html="icons.download"></span> Save JSON</button>
            <div class="pd-menu-box" ref="export">
                <button class="btn btn-sm" :class="errors ? 'btn-outline-danger' : 'btn-success'" :title="errors ? 'В раскладке ошибки, программа её не откроет: смотри Problems' : 'Раскладка для программы: блоб, который открывает dgx_di_pad_open()'" @click="toggle('export')"><span v-html="icons.check"></span> For the program ▾</button>
                <div class="pd-menu pd-export" :class="{ show: menu === 'export' }">
                    <div class="pd-menu-head">Что открывает <code>dgx_di_pad_open()</code>: то же, что делает <code>tools/pad_blob.py</code> из JSON.</div>
                    <a class="dropdown-item" href="#" @click.prevent="savePad"><strong>Blob file</strong> <code>{{ baseName }}.pad</code><span class="pd-about">to embed into the firmware (EMBED_FILES)</span></a>
                    <a class="dropdown-item" href="#" @click.prevent="saveC"><strong>C file</strong> <code>{{ baseName }}.c</code><span class="pd-about">const uint8_t {{ cName }}[], {{ cName }}_size</span></a>
                    <a class="dropdown-item" href="#" @click.prevent="copyC"><strong>Copy the C array</strong><span class="pd-about">the same, into the clipboard</span></a>
                </div>
            </div>
            <a style="display: none" :href="downloadUrl" ref="download"></a>
            <span class="pd-sep"></span>
            <button class="btn btn-outline-secondary btn-sm" :title="'Отменить последнее изменение (Ctrl+Z); можно отменить ' + padStore.history.length" :disabled="!padStore.history.length" @click="padStore.undo()"><span v-html="icons.undo"></span> Undo</button>
            <button class="btn btn-outline-secondary btn-sm" :title="'Вернуть отменённое (Ctrl+Shift+Z или Ctrl+Y); можно вернуть ' + padStore.future.length" :disabled="!padStore.future.length" @click="padStore.redo()"><span v-html="icons.redo"></span> Redo</button>
            <button class="btn btn-outline-secondary btn-sm" :title="'Отменить все правки: назад к раскладке, как её открыли, взяли из заготовки или сохранили в JSON (' + padStore.origin.from + '); сам возврат тоже отменяется'" :disabled="!padStore.changed" @click="padStore.revert()"><span v-html="icons.revert"></span> Revert</button>
            <span class="pd-sep"></span>
            <div class="btn-group btn-group-sm">
                <button class="btn" :class="padStore.edit.mode === 'edit' ? 'btn-primary' : 'btn-outline-primary'" title="Правка: элементы двигать и менять" @click="mode('edit')"><span v-html="icons.move"></span> Edit</button>
                <button class="btn" :class="padStore.edit.mode === 'try' ? 'btn-primary' : 'btn-outline-primary'" title="Проба: мышь — палец, как DGX-DI возьмёт его; справа — что получает программа" @click="mode('try')"><span v-html="icons.hand"></span> Try</button>
            </div>
            <span class="pd-sep"></span>
            <select class="form-select form-select-sm pd-zoom" v-model.number="padStore.edit.zoom" @change="padStore.save()" title="Масштаб: по окну или пикселей экрана на пиксель дисплея">
                <option :value="0">fit</option><option :value="0.5">½</option><option :value="1">1:1</option><option :value="2">2:1</option>
            </select>
            <button class="btn btn-sm" :class="padStore.edit.grid ? 'btn-secondary' : 'btn-outline-secondary'" title="Сетка сантиметров поверх дисплея" @click="padStore.edit.grid = !padStore.edit.grid; padStore.save()" v-html="icons.grid"></button>
            <label class="pd-name mb-0" title="Имя раскладки: в блобе и в имени файла">name <input class="form-control form-control-sm" :value="padStore.layout.name" placeholder="my pad" @change="padStore.change(l => { l.name = $event.target.value; })"></label>
            <span class="ms-auto"></span>
            <button class="btn btn-outline-secondary btn-sm" title="Как пользоваться (H или ?)" @click="help = !help" v-html="icons.help"></button>
        </div>
        <div class="pd-main">
            <PictureEditor v-if="drawing" :key="padStore.edit.item"></PictureEditor>
            <PadCanvas v-else></PadCanvas>
        </div>
        <PadTrial v-if="padStore.edit.mode === 'try'" class="pd-side"></PadTrial>
        <PadProps v-else class="pd-side"></PadProps>
        <div class="pd-status"><span>{{ status }}</span><span class="ms-auto pd-message" :class="'pd-level-' + padStore.level">{{ padStore.message }}</span>
            <button v-if="padStore.undoable && padStore.history.length" class="btn btn-outline-secondary btn-sm pd-undo-offer" title="Отменить это (Ctrl+Z)" @click="padStore.undo()"><span v-html="icons.undo"></span> Undo</button></div>
        <div v-if="help" class="pd-help" @click.self="help = false">
            <div class="pd-help-card">
                <div class="pd-help-head"><strong>Редактор геймпада DGX-DI</strong><button class="btn btn-sm btn-outline-secondary ms-auto" @click="help = false" v-html="icons.close"></button></div>
                <p>Раскладка — это где на дисплее лежат кнопки, крестовины и стики и как они выглядят. Всё в миллиметрах: на другом дисплее она ляжет так же. Программа открывает её блобом: <code>dgx_di_pad_open()</code>.</p>
                <dl>
                    <dt>Начать</dt><dd><b>New</b> — заготовка: свои раскладки DGX-DI или пульты обычных игр. <b>Open</b> — свой JSON, сохранённый здесь раньше (или бросить файл на окно). Всё, что сделано, браузер помнит и без сохранения.</dd>
                    <dt>Дисплей</dt><dd>Плата из списка или свои пиксели и миллиметры. Под геймпадом — придуманная игра, ровный цвет или свой кадр; <b>alpha</b> — та, что даёшь <code>dgx_di_pad_show()</code>. Холст показывает то, что покажет дисплей: те же цвета, то же смешивание.</dd>
                    <dt>Элементы</dt><dd>Тянуть за середину — двигать, за обод — менять размер, миллиметр за шаг (с Alt — десятая). Стрелки — на 1 мм (Shift 5, Alt 0.1). <b>Delete</b> убирает, <b>Ctrl+D</b> копирует. Место отсчитывается от края или от середины дисплея — выбери клетку в <b>Place</b>. Круг пунктиром — за него палец берёт элемент, что бы ни было нарисовано. Кнопка меньше 8 мм поперёк — промахи.</dd>
                    <dt>Вид</dt><dd><b>Look of every item</b> — тема всем сразу; <b>Look</b> у элемента — ему одному. <b>Draw its pictures…</b> — нарисовать самому: у кнопки обычный и нажатый рисунок, у крестовины — обычный и нажатые лучи и углы, у стика — лунка, шляпка и шляпка нажатая. Фигуры в миллиметрах от середины: круг, кольцо, прямоугольник, многоугольник, линия от руки; цвет свой или цвет элемента, светлее, темнее.</dd>
                    <dt>Проба</dt><dd><b>Try</b> — мышь палец: нажимай кнопки, веди по крестовине и стику. Справа — какие биты и оси получает программа.</dd>
                    <dt>В программу</dt><dd><b>For the program</b> — блоб файлом <code>.pad</code> (вложить через <code>EMBED_FILES</code>) или массивом C. <b>Save JSON</b> — чтобы открыть здесь снова; из JSON тот же блоб делает <code>tools/pad_blob.py</code>. Раскладку с ошибками (красным в Problems) программа не откроет.</dd>
                    <dt>Отмена</dt><dd><b>Undo</b> и <b>Redo</b> наверху (Ctrl+Z, Ctrl+Shift+Z или Ctrl+Y) отменяют и возвращают любое изменение раскладки: убранный элемент, сдвиг, тему, рисунок, заготовку. После убранного элемента кнопка Undo есть и в строке внизу. История живёт, пока открыта вкладка, и переживает перезагрузку страницы. <b>Revert</b> отменяет все правки разом: назад к раскладке, как её открыли из файла, взяли из заготовки или в последний раз сохранили в JSON; этот шаг тоже отменяется.</dd>
                    <dt>Клавиши</dt><dd>Ctrl+Z, Ctrl+Shift+Z — отменить, вернуть · Delete · Ctrl+D · стрелки · Esc — закрыть · H или ? — эта справка.</dd>
                </dl>
            </div>
        </div>
    </div>`
};
