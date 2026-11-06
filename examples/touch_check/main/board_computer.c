/*
 * The computer: a window of DGX (dgx_host_screen.h) in place of the display of the Guition JC4827W543 — its glass of
 * 480x272, 95x54 mm, its QSPI at 40 MHz — and the mouse over it as the finger (dgx_di_host.h): the check as that board
 * shows it, without flashing one. Built with make (the Makefile beside main/) or by ESP-IDF for its linux target.
 *
 * The mouse is one finger: the gestures and the tasks of the pad for two fingers and more are left out.
 */
#include "sdkconfig.h"

#if CONFIG_TOUCH_CHECK_BOARD_COMPUTER

#include "board.h"
#include "dgx_di_host.h"
#include "dgx_host_screen.h"

const char board_name[] = "the computer, as a JC4827W543";
const int  board_mm_w = 95, board_mm_h = 54;

/* the JC4827W543's pad (board_jc4827w543.c): the classic one's places, made smaller for its 54 mm */
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

/* the mouse lies on the glass as it is: a calibration of none, to go to the menu at once (it can still be measured there) */
static const dgx_di_calibration_t calibration = {.version = 1, .size = sizeof(dgx_di_calibration_t), .x_min = 0, .x_max = 479, .y_min = 0,
                                                 .y_max = 271, .width = 480, .height = 272};
const dgx_di_calibration_t *const board_calibration = &calibration;

static dgx_screen_t       *display;
static dgx_di_host_touch_t touch;

dgx_screen_t *board_display(void)
{
    static const dgx_host_screen_options_t qspi = {.bus_lines = 4, .title = "touch_check"};
    return display = dgx_host_screen_init(480, 272, 16, &qspi);
}

/* the mouse over the window board_display() made */
dgx_di_device_t *board_touch(void)
{
    if (!display) return NULL;
    dgx_di_host_touch_init(&touch, display);
    return &touch.device;
}

#endif
