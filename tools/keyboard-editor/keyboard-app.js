var app = VueDemi.createApp({
    template: `<KeyboardEdit></KeyboardEdit>`
});
app.component('KeyboardEdit', KeyboardEdit);
app.component('KeyboardCanvas', KeyboardCanvas);
app.component('KeyboardProps', KeyboardProps);
app.use(Pinia.createPinia());
useKeyboardStore().restore();
app.mount('#vue-app');
