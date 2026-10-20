# DGX-DI, display input: the glue that makes the fingers of a device fingers of the display and the gestures of a
# touch (dgx_di.h, plain C over a screen of DGX), and drivers of touch panels ESP-IDF has none for (dgx_di_gsl3680.h).
idf_component_register(
	SRCS "src/dgx_di_pointer.c" "src/dgx_di_touch.c" "src/dgx_di_rects.c" "src/dgx_di_pad.c" "src/dgx_di_pad_layouts.c" "src/dgx_di_keyboard.c" "src/dgx_di_keyboard_layouts.c" "src/dgx_di_calibration.c" "src/dgx_di_calibrate.c" "src/dgx_di_gsl3680.c"
	INCLUDE_DIRS "include"
	REQUIRES jef-sure__dgx esp_driver_i2c
	PRIV_REQUIRES esp_driver_gpio esp_timer
)
