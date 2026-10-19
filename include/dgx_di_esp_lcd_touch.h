#pragma once

#include "dgx_di.h"
#include "esp_lcd_touch.h"

#ifdef __cplusplus
extern "C" {
#endif

/*
 * A touch panel with a driver of ESP-IDF's esp_lcd_touch (GT911, FT5x06, CST816, XPT2046 and the rest) as a device of
 * DGX-DI. All of it static inline: it is compiled only where it is included, so DGX-DI needs no esp_lcd_touch, the
 * program that includes this needs it and its panel's driver.
 *
 * The driver's own mirroring and swapping are better left off (its config's flags 0): the pointer does them, and a
 * calibration finds them. Its numbers of fingers (track_id) are taken when it gives any: give 0 for a driver that does
 * not (DGX-DI follows them by nearness then). Its strength is the size of a point.
 */
typedef struct {
    dgx_di_device_t        device; /* what a pointer reads: dgx_di_pointer_init(&pointer, &touch.device, ...) */
    esp_lcd_touch_handle_t handle;
    bool                   numbered; /* the driver's track_id is a finger's own number */
} dgx_di_esp_lcd_touch_t;

/* the fingers on the panel now: so many, at most max (a dgx_di_read_fn) */
static inline int dgx_di_esp_lcd_touch_read(dgx_di_device_t *device, dgx_di_point_t *points, int max)
{
    dgx_di_esp_lcd_touch_t    *t = (dgx_di_esp_lcd_touch_t *)device;
    esp_lcd_touch_point_data_t data[DGX_DI_FINGERS];
    uint8_t                    n = 0;
    if (max > DGX_DI_FINGERS) max = DGX_DI_FINGERS;
    if (!t->handle || max <= 0 || esp_lcd_touch_read_data(t->handle) != ESP_OK || esp_lcd_touch_get_data(t->handle, data, &n, (uint8_t)max) != ESP_OK)
        return 0;
    for (int i = 0; i < n; ++i)
        points[i] = (dgx_di_point_t){.x = (int16_t)data[i].x, .y = (int16_t)data[i].y, .size = data[i].strength,
                                     .id = t->numbered ? (uint8_t)data[i].track_id : (uint8_t)DGX_DI_NO_ID};
    return n;
}

/*
 * The panel of a driver made already (esp_lcd_touch_new_...): it sees so many fingers at once (its driver's most), its
 * coordinates 0..x_max-1 and 0..y_max-1 of its config; numbered: its track_id stays with a finger.
 */
static inline void dgx_di_esp_lcd_touch_init(dgx_di_esp_lcd_touch_t *t, esp_lcd_touch_handle_t handle, int fingers, bool numbered)
{
    t->handle = handle, t->numbered = numbered;
    t->device = (dgx_di_device_t){.read = dgx_di_esp_lcd_touch_read, .fingers = (uint8_t)(fingers > 0 ? fingers : 1), .x_min = 0,
                                  .x_max = (int16_t)(handle->config.x_max - 1), .y_min = 0, .y_max = (int16_t)(handle->config.y_max - 1)};
}

#ifdef __cplusplus
}
#endif
