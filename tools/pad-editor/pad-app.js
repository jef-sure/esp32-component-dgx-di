var app = VueDemi.createApp({
    template: `<PadEdit></PadEdit>`
});
app.component('PadEdit', PadEdit);
app.component('PadCanvas', PadCanvas);
app.component('PadProps', PadProps);
app.component('PictureEditor', PictureEditor);
app.use(Pinia.createPinia());
usePadStore().restore();
app.mount('#vue-app');
