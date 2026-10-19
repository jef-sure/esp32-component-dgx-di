#pragma once

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include "dgx_di.h"
#include "driver/i2c_master.h"

#ifdef __cplusplus
extern "C" {
#endif

/*
 * A Silead GSL3680 capacitive touch panel on I2C (0x40): up to 10 fingers. Their numbers are not given: the chip's do not
 * stay with a finger (its vendor makes them do by a GPL algorithm), the pointer follows the fingers by nearness.
 *
 * The chip has no firmware of its own: init writes it in from the data given (tools/gsl_firmware.py: pages of 32
 * words, and the range of the coordinates the firmware gives, which is the panel's: 64 a channel of its sensor), then
 * waits for the chip to say it runs. The bus is the board's, set up before and maybe shared: the driver only adds its
 * device to it. No interrupt: the caller reads as often as it looks (a step of its loop). A pointer of dgx_di.h makes
 * the points those of the display.
 *
 * RAM: this struct; the firmware is read where it lies (flash).
 */
typedef struct {
    dgx_di_device_t         device; /* what a pointer reads: dgx_di_pointer_init(&pointer, &gsl.device, ...) */
    i2c_master_dev_handle_t dev;
} dgx_di_gsl3680_t;

/*
 * The chip reset (reset_gpio, active low, -1: not wired; int_gpio held low meanwhile picks the address 0x40, -1: not
 * wired), its firmware written in at scl_hz (0: 400 kHz) and started; false when it does not answer, the firmware is
 * not such data or it does not start.
 */
bool dgx_di_gsl3680_init(dgx_di_gsl3680_t *touch, i2c_master_bus_handle_t bus, int reset_gpio, int int_gpio, int scl_hz,
                         const uint8_t *firmware, size_t size);

#ifdef __cplusplus
}
#endif
