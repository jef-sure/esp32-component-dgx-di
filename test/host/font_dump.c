/*
 * Text drawn by DGX itself, as JSON: of a few of DGX's own fonts, the bounds of some strings and their pixels drawn from
 * a baseline; and the labels of the ready Latin keyboard on the 7" display in one of them, key by key. The fonts are
 * read from the same C by tools/keyboard-editor/Fonts.js, and tools/keyboard-editor/test.js compares: the editor must
 * draw a label of a font of DGX pixel for pixel where DGX-DI draws it.
 */
#include <stdio.h>

#include "dgx_di_keyboard.h"
#include "dgx_draw.h"
#include "dgx_font.h"
#include "drivers/vscreen.h"

dgx_font_t *LiberationSansRegular12(void);
dgx_font_t *TerminusTTFMedium12(void);
dgx_font_t *DejaVuSansBook32(void);

static const struct {
    const char *file;
    dgx_font_t *(*font)(void);
} fonts[] = {{"LiberationSansRegular12.c", LiberationSansRegular12}, {"TerminusTTFMedium12.c", TerminusTTFMedium12}, {"DejaVuSansBook32.c", DejaVuSansBook32}};

/* letters of a font, letters past it (a glyph missing advances by its first: a letter after one), several scripts */
static const char *const strings[] = {"H",   "Ag", "1/2", "\xc3\xa4\xc3\x9f\xe2\x82\xac", "\xd0\x9f\xd1\x80\xd0\xb8\xd0\xb2\xd0\xb5\xd1\x82", "q Q",
                                      "x\xe2\x82\xacx", "\xd0\x9fq"};

/* the pixels of a color in a rectangle of the screen, as [x, y] pairs */
static void pixels(dgx_screen_t *s, int x0, int y0, int w, int h, uint32_t color)
{
    bool first = true;
    printf("[");
    for (int y = y0; y < y0 + h; ++y)
        for (int x = x0; x < x0 + w; ++x)
            if (s->get_pixel(s, x, y) == color) printf("%s[%d, %d]", first ? "" : ", ", x, y), first = false;
    printf("]");
}

static void json_string(const char *s)
{
    printf("\"");
    for (; *s; ++s) printf(*s == '"' || *s == '\\' ? "\\%c" : "%c", *s);
    printf("\"");
}

int main(void)
{
    dgx_screen_t *s = dgx_vscreen_init(1024, 600, 16, DgxScreenRGB);
    printf("{\"fonts\": [\n");
    for (size_t f = 0; f < sizeof fonts / sizeof *fonts; ++f) {
        dgx_font_t *font = fonts[f].font();
        printf("%s  {\"file\": \"%s\", \"strings\": [", f ? ",\n" : "", fonts[f].file);
        for (size_t i = 0; i < sizeof strings / sizeof *strings; ++i) {
            int16_t   top = 0, height = 0;
            const int width = dgx_font_string_bounds(strings[i], font, &top, &height);
            dgx_fill_rectangle(s, 0, 0, 1024, 200, 0);
            dgx_font_string_utf8_screen(s, 10, 100, strings[i], 0xFFFF, DgxOutputNormal, 1, font, NULL, NULL);
            printf("%s\n    {\"text\": ", i ? "," : "");
            json_string(strings[i]);
            printf(", \"width\": %d, \"top\": %d, \"height\": %d, \"from\": [10, 100], \"pixels\": ", width, top, height);
            pixels(s, 0, 0, 1024, 200, 0xFFFF);
            printf("}");
        }
        printf("]}");
    }
    /*
     * the Latin keyboard on the 7" in DejaVu Sans 32 and in Liberation Sans 12 (its capital of an odd height): its keys' labels, white,
     * each in its key (the signs too: told apart by the editor)
     */
    printf("\n], \"keyboards\": [");
    for (int f = 0; f < 2; ++f) {
        dgx_di_keyboard_t kb;
        dgx_fill_rectangle(s, 0, 0, 1024, 600, 0);
        const bool open = dgx_di_keyboard_open(&kb, s, 154, 86, dgx_di_keyboard_latin, dgx_di_keyboard_latin_size, f ? LiberationSansRegular12() : DejaVuSansBook32(), NULL);
        printf("%s\n  {\"layout\": \"latin\", \"font\": \"%s\", \"display\": {\"width\": 1024, \"height\": 600, \"widthMm\": 154, \"heightMm\": 86}, \"open\": %s, \"keys\": [",
               f ? "," : "", f ? "LiberationSansRegular12.c" : "DejaVuSansBook32.c", open ? "true" : "false");
        for (int k = 0; open && k < kb.count; ++k) {
            const dgx_di_rect_t *r = &kb.keys[k];
            printf("%s\n    {\"rect\": [%d, %d, %d, %d], \"pixels\": ", k ? "," : "", r->x, r->y, r->w, r->h);
            pixels(s, r->x, r->y, r->w, r->h, 0xFFFF);
            printf("}");
        }
        printf("]}");
    }
    printf("]}\n");
    dgx_screen_destroy(&s);
    return 0;
}
