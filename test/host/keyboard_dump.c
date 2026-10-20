/*
 * The geometry of DGX-DI's ready keyboard layouts as the C makes it, as JSON: the band and the keys of every page on
 * several displays, with no field and with one at the bottom. tools/keyboard-editor/test.js makes the same of the JSON
 * of the layouts and compares: the editor must show the keys where DGX-DI puts them.
 */
#include <stdio.h>

#include "../../src/dgx_di_keyboard.c" /* its lay() of each page */
#include "drivers/vscreen.h"

static const struct {
    int w, h, mm_w, mm_h;
} displays[] = {{1024, 600, 154, 86}, {1280, 800, 217, 136}, {320, 240, 58, 43}, {600, 1024, 86, 154}, {480, 272, 95, 54}, {800, 480, 108, 65}};

static const struct {
    const char    *name;
    const uint8_t *blob;
    const size_t  *size;
} layouts[] = {{"digits", dgx_di_keyboard_digits, &dgx_di_keyboard_digits_size}, {"latin", dgx_di_keyboard_latin, &dgx_di_keyboard_latin_size}};

int main(void)
{
    bool first = true;
    printf("[\n");
    for (size_t l = 0; l < sizeof layouts / sizeof *layouts; ++l)
        for (size_t d = 0; d < sizeof displays / sizeof *displays; ++d)
            for (int with_field = 0; with_field < 2; ++with_field) {
                dgx_screen_t       *s     = dgx_vscreen_init(displays[d].w, displays[d].h, 16, DgxScreenRGB);
                const dgx_di_rect_t field = {.x = 0, .y = (int16_t)(displays[d].h - 60), .w = (int16_t)displays[d].w, .h = 50};
                dgx_di_keyboard_t   kb;
                const bool open = dgx_di_keyboard_open(&kb, s, displays[d].mm_w, displays[d].mm_h, layouts[l].blob, *layouts[l].size, NULL, with_field ? &field : NULL);
                printf("%s  {\"layout\": \"%s\", \"display\": {\"width\": %d, \"height\": %d, \"widthMm\": %d, \"heightMm\": %d}, ", first ? "" : ",\n",
                       layouts[l].name, displays[d].w, displays[d].h, displays[d].mm_w, displays[d].mm_h);
                first = false;
                if (with_field) printf("\"field\": {\"x\": %d, \"y\": %d, \"w\": %d, \"h\": %d}, ", field.x, field.y, field.w, field.h);
                else printf("\"field\": null, ");
                if (!open) {
                    printf("\"band\": null, \"pages\": []}");
                    dgx_screen_destroy(&s);
                    continue;
                }
                printf("\"band\": {\"x\": %d, \"y\": %d, \"w\": %d, \"h\": %d, \"unit\": %d, \"gapX\": %d, \"gapY\": %d, \"keyH\": %d, \"marginX\": %d, "
                       "\"marginY\": %d, \"radius\": %d, \"rows\": %d}, \"pages\": [",
                       kb.x, kb.y, kb.w, kb.h, kb.unit, kb.gap_x, kb.gap_y, kb.key_h, kb.margin_x, kb.margin_y, kb.radius, kb.rows);
                for (int p = 0; p < kb.pages; ++p) {
                    lay(&kb, p);
                    printf("%s[", p ? ", " : "");
                    for (int k = 0; k < kb.count; ++k) printf("%s[%d, %d, %d, %d]", k ? ", " : "", kb.keys[k].x, kb.keys[k].y, kb.keys[k].w, kb.keys[k].h);
                    printf("]");
                }
                printf("]}");
                dgx_screen_destroy(&s);
            }
    printf("\n]\n");
    return 0;
}
