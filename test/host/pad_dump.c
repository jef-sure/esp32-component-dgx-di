/*
 * The places of the items of DGX-DI's ready pad layouts as the C makes them, and colors mixed as DGX mixes a layer, as
 * JSON: tools/pad-editor/test.js makes the same of the JSON of the layouts and compares — the editor must show the
 * items where DGX-DI puts them, in the colors the display shows.
 */
#include <stdio.h>

#include "bus/dgx_bus_protocols.h"
#include "dgx_di_pad.h"
#include "drivers/vscreen.h"

static const struct {
    int w, h, mm_w, mm_h;
} displays[] = {{1024, 600, 154, 86}, {1280, 800, 217, 136}, {320, 240, 58, 43}, {600, 1024, 86, 154}, {480, 272, 95, 54}, {800, 480, 108, 65}};

static const struct {
    const char    *name;
    const uint8_t *blob;
    const size_t  *size;
} layouts[] = {{"classic", dgx_di_pad_layout_classic, &dgx_di_pad_layout_classic_size},
               {"flat", dgx_di_pad_layout_flat, &dgx_di_pad_layout_flat_size},
               {"twin", dgx_di_pad_layout_twin, &dgx_di_pad_layout_twin_size}};

int main(void)
{
    printf("{\"places\": [\n");
    bool first = true;
    for (size_t l = 0; l < sizeof layouts / sizeof *layouts; ++l)
        for (size_t d = 0; d < sizeof displays / sizeof *displays; ++d) {
            dgx_screen_t *s = dgx_vscreen_init(displays[d].w, displays[d].h, 16, DgxScreenRGB);
            dgx_di_pad_t  pad;
            if (!dgx_di_pad_open(&pad, s, displays[d].mm_w, displays[d].mm_h, layouts[l].blob, *layouts[l].size)) return 1;
            printf("%s  {\"layout\": \"%s\", \"display\": {\"width\": %d, \"height\": %d, \"widthMm\": %d, \"heightMm\": %d}, \"items\": [", first ? "" : ",\n",
                   layouts[l].name, displays[d].w, displays[d].h, displays[d].mm_w, displays[d].mm_h);
            first = false;
            for (int k = 0; k < pad.count; ++k) printf("%s[%d, %d, %d]", k ? ", " : "", pad.places[k].x, pad.places[k].y, pad.places[k].r);
            printf("]}");
            dgx_di_pad_free(&pad, NULL);
            dgx_screen_destroy(&s);
        }
    /* a layer's color over a color under it, at weights of alpha as dgx_overlay_set() makes them */
    static const uint16_t colors[] = {0x0000, 0xffff, 0x2e65, 0xd8c3, 0x249d, 0xee24, 0x5acb, 0x7bef, 0xf800, 0x07e0, 0x001f, 0x8410};
    static const int      alphas[] = {0, 1, 64, 127, 128, 160, 200, 254, 255};
    printf("\n], \"mixed\": [\n");
    first = true;
    for (size_t a = 0; a < sizeof alphas / sizeof *alphas; ++a)
        for (size_t u = 0; u < sizeof colors / sizeof *colors; ++u)
            for (size_t c = 0; c < sizeof colors / sizeof *colors; ++c) {
                const uint32_t weight = ((uint32_t)alphas[a] * 32u + 127u) / 255u;
                const uint32_t pre    = ((colors[c] | (uint32_t)colors[c] << 16) & 0x07E0F81Fu) * weight;
                printf("%s[%d, %d, %d, %d]", first ? "  " : ", ", alphas[a], colors[u], colors[c], dgx_bus_overlay_mix(colors[u], pre, 32u - weight));
                first = false;
            }
    printf("\n]}\n");
    return 0;
}
