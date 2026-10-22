var app = VueDemi.createApp({
    template: `<KeyboardEdit></KeyboardEdit>`
});
app.component('KeyboardEdit', KeyboardEdit);
app.component('KeyboardCanvas', KeyboardCanvas);
app.component('KeyboardProps', KeyboardProps);
app.use(Pinia.createPinia());
useKeyboardStore().restore();
useKeyboardStore().restoreFont();
app.mount('#vue-app');
