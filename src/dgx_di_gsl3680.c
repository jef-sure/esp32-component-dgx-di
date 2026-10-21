/*
 * The GSL3680 started as its vendor's driver does (Guition's esp_lcd_touch_gsl3680 for the JC8012P4A1C): reset, a
 * check that it answers, its registers cleared, the firmware written page by page, started twice, then 0x5a5a5a5a at
 * 0xb0 when it runs. Points at 0x80: the count of fingers, 3 bytes more, then 4 bytes a finger (y low, y high, x low,
 * x high; 12 bits each). The 4 bits above x number the fingers, but not each finger the same from read to read: the
 * vendor's driver makes them so by its own algorithm (gsl_alg_id_main, GPL), and a second finger down takes the first's
 * number now and then (seen on the JC8012P4A1C). So none are given: the pointer follows the fingers by nearness.
 */
#include "dgx_di_gsl3680.h"

#include <string.h>

#include "driver/gpio.h"
#include "esp_log.h"
#include "esp_timer.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

static const char *TAG = "gsl3680";

enum { GSL_ADDR = 0x40, GSL_HZ = 400000, GSL_TIMEOUT_MS = 50, GSL_FINGERS = 10, PAGE_WORDS = 32, PAGE_BYTES = 4 * PAGE_WORDS };
enum { HEAD_BYTES = 10 }; /* "GSLF", u16 pages, u16 width, u16 height */
enum { REG_POINTS = 0x80, REG_RESET = 0x88, REG_STATUS = 0xb0, REG_BC = 0xbc, REG_START = 0xe0, REG_E4 = 0xe4, REG_PAGE = 0xf0 };

static void wait_ms(int ms)
{
    vTaskDelay(pdMS_TO_TICKS(ms) ? pdMS_TO_TICKS(ms) : 1);
}

/* bytes from reg on: one write, the register first */
static bool put(dgx_di_gsl3680_t *t, uint8_t reg, const void *data, size_t n)
{
    uint8_t buf[1 + PAGE_BYTES];
    buf[0] = reg;
    memcpy(buf + 1, data, n);
    return i2c_master_transmit(t->dev, buf, 1 + n, GSL_TIMEOUT_MS) == ESP_OK;
}

static bool put1(dgx_di_gsl3680_t *t, uint8_t reg, uint8_t value)
{
    return put(t, reg, &value, 1);
}

static bool get(dgx_di_gsl3680_t *t, uint8_t reg, void *data, size_t n)
{
    return i2c_master_transmit_receive(t->dev, &reg, 1, data, n, GSL_TIMEOUT_MS) == ESP_OK;
}

/* no reset wired: the waits alone, the chip as it came up */
static void pulse_reset(int gpio)
{
    if (gpio >= 0) gpio_set_level((gpio_num_t)gpio, 0);
    wait_ms(20);
    if (gpio >= 0) gpio_set_level((gpio_num_t)gpio, 1);
    wait_ms(20);
}

/* the firmware's pages in at burst bytes a write (128: a page at once; 4: a word at once, as the vendor does) */
static bool load(dgx_di_gsl3680_t *t, const uint8_t *fw, int pages, int burst)
{
    for (int p = 0; p < pages; ++p, fw += 1 + PAGE_BYTES) {
        if (!put1(t, REG_PAGE, fw[0])) return false;
        for (int at = 0; at < PAGE_BYTES; at += burst)
            if (!put(t, (uint8_t)at, fw + 1 + at, (size_t)burst)) return false;
    }
    return true;
}

static bool start(dgx_di_gsl3680_t *t)
{
    const bool ok = put1(t, REG_START, 0x00);
    wait_ms(10);
    return ok;
}

static bool reset(dgx_di_gsl3680_t *t, int reset_gpio)
{
    static const uint8_t zero[4] = {0};
    pulse_reset(reset_gpio);
    bool ok = put1(t, REG_E4, 0x04);
    wait_ms(10);
    ok = ok && put(t, REG_BC, zero, 4);
    wait_ms(10);
    return ok;
}

static bool runs(dgx_di_gsl3680_t *t)
{
    for (int i = 0; i < 10; ++i) {
        wait_ms(50);
        uint8_t s[4];
        if (get(t, REG_STATUS, s, 4) && s[0] == 0x5a && s[1] == 0x5a && s[2] == 0x5a && s[3] == 0x5a) return true;
    }
    return false;
}

/* cleared, the firmware in, started, reset, started again: running or not */
static bool boot(dgx_di_gsl3680_t *t, int reset_gpio, const uint8_t *fw, int pages, int burst)
{
    pulse_reset(reset_gpio);
    bool ok = put1(t, REG_RESET, 0x01);
    wait_ms(5);
    ok = ok && put1(t, REG_E4, 0x04);
    wait_ms(5);
    ok = ok && put1(t, REG_START, 0x00);
    wait_ms(20);
    return ok && reset(t, reset_gpio) && load(t, fw, pages, burst) && start(t) && reset(t, reset_gpio) && start(t) && runs(t);
}

/* the fingers on the chip now: so many of them, at most max (a dgx_di_read_fn) */
static int gsl_read(dgx_di_device_t *device, dgx_di_point_t *points, int max)
{
    dgx_di_gsl3680_t *t = (dgx_di_gsl3680_t *)device;
    uint8_t           d[4 + 4 * GSL_FINGERS];
    if (max > GSL_FINGERS) max = GSL_FINGERS;
    if (!t->dev || max <= 0 || !get(t, REG_POINTS, d, 4 + 4 * (size_t)max)) return 0;
    /* more fingers than it sees: not points (a read as it resets, 0xff throughout), not read */
    if (d[0] > GSL_FINGERS) return 0;
    const int n = d[0] < max ? d[0] : max;
    for (int i = 0; i < n; ++i) {
        const uint8_t *e = d + 4 + 4 * i;
        points[i] = (dgx_di_point_t){.x = (int16_t)((e[3] & 0x0f) << 8 | e[2]), .y = (int16_t)((e[1] & 0x0f) << 8 | e[0]), .id = DGX_DI_NO_ID};
    }
    return n;
}

bool dgx_di_gsl3680_init(dgx_di_gsl3680_t *t, i2c_master_bus_handle_t bus, int reset_gpio, int int_gpio, int scl_hz, const uint8_t *fw,
                         size_t size)
{
    memset(t, 0, sizeof *t);
    const int pages = size >= HEAD_BYTES && !memcmp(fw, "GSLF", 4) ? fw[4] | fw[5] << 8 : -1;
    const int width = size >= HEAD_BYTES ? fw[6] | fw[7] << 8 : 0, height = size >= HEAD_BYTES ? fw[8] | fw[9] << 8 : 0;
    if (pages <= 0 || width <= 0 || height <= 0 || size != HEAD_BYTES + (size_t)pages * (1 + PAGE_BYTES)) {
        ESP_LOGE(TAG, "the firmware is not pages of the chip with its range (tools/gsl_firmware.py)");
        return false;
    }
    fw += HEAD_BYTES;
    t->device = (dgx_di_device_t){.read = gsl_read, .fingers = GSL_FINGERS, .x_min = 0, .x_max = (int16_t)(width - 1), .y_min = 0, .y_max = (int16_t)(height - 1)};
    const i2c_device_config_t dev = {.dev_addr_length = I2C_ADDR_BIT_LEN_7, .device_address = GSL_ADDR, .scl_speed_hz = scl_hz > 0 ? (uint32_t)scl_hz : GSL_HZ};
    if (i2c_master_bus_add_device(bus, &dev, &t->dev) != ESP_OK) return false;
    /* reset with int low: the address 0x40 */
    if (reset_gpio >= 0) gpio_set_direction((gpio_num_t)reset_gpio, GPIO_MODE_OUTPUT), gpio_set_level((gpio_num_t)reset_gpio, 0);
    if (int_gpio >= 0) gpio_set_direction((gpio_num_t)int_gpio, GPIO_MODE_OUTPUT), gpio_set_level((gpio_num_t)int_gpio, 0);
    wait_ms(10);
    if (reset_gpio >= 0) gpio_set_level((gpio_num_t)reset_gpio, 1);
    wait_ms(60);
    if (int_gpio >= 0) gpio_set_direction((gpio_num_t)int_gpio, GPIO_MODE_INPUT);
    /* it answers: a word written to the page register reads back */
    static const uint8_t probe[4] = {0x12, 0x34, 0x56, 0x78};
    uint8_t              back[4];
    if (!put(t, REG_PAGE, probe, 4) || (wait_ms(20), !get(t, REG_PAGE, back, 4)) || memcmp(back, probe, 3)) {
        ESP_LOGE(TAG, "no answer at 0x%02x", GSL_ADDR);
        goto fail;
    }
    /* a page a write; a word a write as the vendor does when the chip does not take that */
    for (int burst = PAGE_BYTES; burst >= 4; burst = burst == PAGE_BYTES ? 4 : 0) {
        const int64_t t0 = esp_timer_get_time();
        if (boot(t, reset_gpio, fw, pages, burst)) {
            ESP_LOGI(TAG, "runs: %d pages of firmware, %d bytes a write, %lld ms; %dx%d", pages, burst, (long long)((esp_timer_get_time() - t0) / 1000),
                     width, height);
            return true;
        }
        ESP_LOGW(TAG, "does not start with %d bytes a write", burst);
    }
fail:
    i2c_master_bus_rm_device(t->dev);
    t->dev = NULL;
    return false;
}
