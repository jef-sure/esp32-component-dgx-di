/*
 * The Guition JC4827W543, 4.3 inches: an ESP32-S3, an NV3041A of 480x272 on QSPI (DGX's dgx_qspi_init and
 * dgx_nv3041a_init, as DGX's README has it), its backlight on GPIO1; a GT911 on I2C (SDA 8, SCL 4), its reset on
 * GPIO38 and its interrupt on GPIO3, the ESP32's to drive: the address is chosen at its reset, 0x5D.
 *
 * Its GT911 gives the pixels of the display, 0..479 and 0..271, though it tells of itself a field of 1085x600 and a
 * configuration of 1024x600 (a 7-inch one's): its range is given here, not read. On one board of these, 2026-10-23,
 * it also answered at 0x14 after the reset.
 */
#include "sdkconfig.h"

#if CONFIG_TOUCH_CHECK_BOARD_JC4827W543

#include "board.h"
#include "bus/dgx_qspi_esp32.h"
#include "dgx_di_esp_lcd_touch.h"
#include "driver/i2c_master.h"
#include "drivers/nv3041a.h"
#include "esp_lcd_touch_gt911.h"
#include "esp_log.h"

const char board_name[] = "Guition JC4827W543, 4.3\"";
const int  board_mm_w = 95, board_mm_h = 54; /* its glass: 95.04 x 53.86 mm, as the panel's sheet has it */

/*
 * The classic pad is for a tablet: its cross of 40 mm would cover the half of these 54 mm and the X and BACK beside it.
 * This one keeps the classic's places, made smaller: a cross of 26 mm under the left thumb, A B X Y of 10 mm under the
 * right, BACK and START between, low (tenths of a millimetre, as dgx_di_pad_classic).
 */
static const dgx_di_pad_item_t small_pad[] = {
    {DGX_DI_PAD_CROSS, DGX_DI_FROM_START, DGX_DI_FROM_END, 0, 170, 170, 130, 0, 0},
    {DGX_DI_PAD_BUTTON, DGX_DI_FROM_END, DGX_DI_FROM_END, 0, 170, 90, 50, DGX_DI_PAD_A, 0x2E65},  /* green */
    {DGX_DI_PAD_BUTTON, DGX_DI_FROM_END, DGX_DI_FROM_END, 0, 90, 170, 50, DGX_DI_PAD_B, 0xD8C3},  /* red */
    {DGX_DI_PAD_BUTTON, DGX_DI_FROM_END, DGX_DI_FROM_END, 0, 250, 170, 50, DGX_DI_PAD_X, 0x249D}, /* blue */
    {DGX_DI_PAD_BUTTON, DGX_DI_FROM_END, DGX_DI_FROM_END, 0, 170, 250, 50, DGX_DI_PAD_Y, 0xEE24}, /* yellow */
    {DGX_DI_PAD_BUTTON, DGX_DI_FROM_MIDDLE, DGX_DI_FROM_END, 0, -120, 80, 45, DGX_DI_PAD_BACK, 0},
    {DGX_DI_PAD_BUTTON, DGX_DI_FROM_MIDDLE, DGX_DI_FROM_END, 0, 120, 80, 45, DGX_DI_PAD_START, 0},
};
const dgx_di_pad_item_t *const board_pad       = small_pad;
const int                      board_pad_count = sizeof small_pad / sizeof small_pad[0];

/*
 * Measured on one board of these under DGX 0.10: 4.4 pixels off on average without it, 2.4 with it. The numbers of
 * 2026-10-23 (flags 3) belonged to DGX 0.8, where MADCTL did not reach the panel and the picture was upside down.
 */
static const dgx_di_calibration_t calibration = {.version = 1, .size = sizeof(dgx_di_calibration_t), .x_min = 0, .x_max = 479, .y_min = 0, .y_max = 271, .width = 480, .height = 272, .flags = 0, .knots_x = 3, .knots_y = 3, .rounds = 3, .affine = {66171, 447, -375665, -881, 67265, -193200}, .x = {{34, 34}, {240, 241}, {445, 445}}, .y = {{34, 33}, {136, 137}, {237, 236}}, .down_ms = 0, .up_ms = 30, .blink_ms = 0, .gap_ms = 0, .off_tenths = 44, .worst_tenths = 90, .left_tenths = 24, .left_worst_tenths = 50};
const dgx_di_calibration_t *const board_calibration = &calibration;

dgx_screen_t *board_display(void)
{
    dgx_bus_protocols_t *bus = dgx_qspi_init(SPI2_HOST, SPI_DMA_CH_AUTO, GPIO_NUM_21, GPIO_NUM_48, GPIO_NUM_40, GPIO_NUM_39, GPIO_NUM_47 /* sclk */,
                                             GPIO_NUM_45 /* cs */, 40 * 1000 * 1000, 0);
    return bus ? dgx_nv3041a_init(bus, GPIO_NUM_NC, GPIO_NUM_1, 16, DgxScreenRGB) : NULL;
}

static dgx_di_esp_lcd_touch_t touch;

dgx_di_device_t *board_touch(void)
{
    const i2c_master_bus_config_t bus_config = {.i2c_port = I2C_NUM_0, .sda_io_num = GPIO_NUM_8, .scl_io_num = GPIO_NUM_4,
                                                .clk_source = I2C_CLK_SRC_DEFAULT, .glitch_ignore_cnt = 7, .flags.enable_internal_pullup = true};
    i2c_master_bus_handle_t       i2c;
    if (i2c_new_master_bus(&bus_config, &i2c) != ESP_OK) return NULL;
    esp_lcd_panel_io_handle_t     io;
    esp_lcd_panel_io_i2c_config_t io_config = ESP_LCD_TOUCH_IO_I2C_GT911_CONFIG();
    io_config.scl_speed_hz                  = 400000;
    if (esp_lcd_new_panel_io_i2c(i2c, &io_config, &io) != ESP_OK) return NULL;
    /* the address its interrupt's level sets at the reset */
    static esp_lcd_touch_io_gt911_config_t address = {.dev_addr = ESP_LCD_TOUCH_IO_I2C_GT911_ADDRESS};
    const esp_lcd_touch_config_t           config  = {.x_max = 480, .y_max = 272, .rst_gpio_num = GPIO_NUM_38, .int_gpio_num = GPIO_NUM_3,
                                                      .driver_data = &address};
    esp_lcd_touch_handle_t                 handle;
    if (esp_lcd_touch_new_i2c_gt911(io, &config, &handle) != ESP_OK) {
        ESP_LOGE("board", "no GT911 at 0x%02X", address.dev_addr);
        return NULL;
    }
    dgx_di_esp_lcd_touch_init(&touch, handle, 5, true);
    return &touch.device;
}

#endif
