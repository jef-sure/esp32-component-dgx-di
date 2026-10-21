var app = VueDemi.createApp({
    template: `<PadEdit></PadEdit>`
});
app.component('PadEdit', PadEdit);
app.component('PadCanvas', PadCanvas);
app.component('PadProps', PadProps);
app.component('PadTrial', PadTrial);
app.component('PictureEditor', PictureEditor);
app.component('PadThumb', PadThumb);
app.component('ItemThumb', ItemThumb);
app.component('AnchorPicker', AnchorPicker);
app.use(Pinia.createPinia());
usePadStore().restore();
app.mount('#vue-app');
