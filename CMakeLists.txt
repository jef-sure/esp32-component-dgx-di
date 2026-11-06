# DGX-DI, display input: the glue that makes the fingers of a device fingers of the display and the gestures of a
# touch (dgx_di.h, plain C over a screen of DGX), and drivers of touch panels ESP-IDF has none for (dgx_di_gsl3680.h).
set(srcs "src/dgx_di_pointer.c" "src/dgx_di_touch.c" "src/dgx_di_rects.c" "src/dgx_di_pad.c" "src/dgx_di_pad_layouts.c"
	"src/dgx_di_keyboard.c" "src/dgx_di_keyboard_layouts.c" "src/dgx_di_calibration.c" "src/dgx_di_calibrate.c")

# The linux target of ESP-IDF: the program on the computer, its displays windows of DGX (DGX 0.10.8 and later) and its
# touch the mouse over them (dgx_di_host.h). The GSL3680 is a chip on the I2C of a board: not built there.
idf_build_get_property(dgx_di_target IDF_TARGET)
if(dgx_di_target STREQUAL "linux")
	set(requires jef-sure__dgx)
	set(priv_requires esp_timer)
else()
	list(APPEND srcs "src/dgx_di_gsl3680.c")
	set(requires jef-sure__dgx esp_driver_i2c)
	set(priv_requires esp_driver_gpio esp_timer)
endif()

idf_component_register(
	SRCS ${srcs}
	INCLUDE_DIRS "include"
	REQUIRES ${requires}
	PRIV_REQUIRES ${priv_requires}
)
