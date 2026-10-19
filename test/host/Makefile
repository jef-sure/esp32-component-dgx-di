# Host tests of DGX-DI, the part that is plain C (the glue of a pointer, the gestures of a touch).
# Run: make -C test/host
#
# The headers of DGX (a screen: its size and orientation) are taken from the managed component of the project DGX-DI
# sits in, else from a checkout next to DGX-DI's (../esp32-component-dgx); DGX=<path> takes another copy.
ROOT   := ../..
DGX    ?= $(firstword $(wildcard $(ROOT)/../../managed_components/jef-sure__dgx) $(ROOT)/../esp32-component-dgx)
CC     ?= gcc
CFLAGS ?= -O1 -g -fsanitize=address,undefined -fno-sanitize-recover=undefined -fno-omit-frame-pointer
CFLAGS += -std=gnu11 -Wall -Wextra -Wno-unused-parameter -I$(ROOT)/include -I$(DGX)/include
BUILD  := build

all: run-test_pointer run-test_touch run-test_calibration run-test_rects run-test_pad

run-%: $(BUILD)/%
	./$<

$(BUILD)/test_pointer: test_pointer.c check.h $(ROOT)/src/dgx_di_pointer.c $(ROOT)/include/dgx_di.h
	mkdir -p $(BUILD)
	$(CC) $(CFLAGS) -o $@ test_pointer.c $(ROOT)/src/dgx_di_pointer.c

$(BUILD)/test_calibration: test_calibration.c check.h $(ROOT)/src/dgx_di_calibration.c $(ROOT)/src/dgx_di_pointer.c $(ROOT)/include/dgx_di.h
	mkdir -p $(BUILD)
	$(CC) $(CFLAGS) -o $@ test_calibration.c $(ROOT)/src/dgx_di_calibration.c $(ROOT)/src/dgx_di_pointer.c -lm

$(BUILD)/test_touch: test_touch.c check.h $(ROOT)/src/dgx_di_touch.c $(ROOT)/include/dgx_di.h
	mkdir -p $(BUILD)
	$(CC) $(CFLAGS) -o $@ test_touch.c $(ROOT)/src/dgx_di_touch.c

$(BUILD)/test_rects: test_rects.c check.h $(ROOT)/src/dgx_di_rects.c $(ROOT)/src/dgx_di_pointer.c $(ROOT)/include/dgx_di.h
	mkdir -p $(BUILD)
	$(CC) $(CFLAGS) -o $@ test_rects.c $(ROOT)/src/dgx_di_rects.c $(ROOT)/src/dgx_di_pointer.c

# the pad draws with DGX: its sources built for the host as its own tests build them
DGX_SRCS  := $(addprefix $(DGX)/src/, pixels.c draw.c ellipse.c bezier.c texture.c orientation.c drivers/vscreen.c lut.c indexed.c overlay.c \
               dgx_bus_optimized_funcs.c dgx_spi_area_protocol.c bitmap.c font.c bw_screen.c scroll.c bus/fb_area.c bus/fb_tiles.c bus/fb_flip.c)
DGX_FLAGS := -I$(DGX)/test/host/stubs -I$(DGX)/src -include $(DGX)/test/host/stubs/esp_log.h -Wno-format -Wno-sign-compare

$(BUILD)/test_pad: test_pad.c check.h $(ROOT)/src/dgx_di_pad.c $(ROOT)/src/dgx_di_pointer.c $(ROOT)/include/dgx_di.h $(ROOT)/include/dgx_di_pad.h
	mkdir -p $(BUILD)
	$(CC) $(CFLAGS) $(DGX_FLAGS) -o $@ test_pad.c $(ROOT)/src/dgx_di_pad.c $(ROOT)/src/dgx_di_pointer.c $(DGX_SRCS) -lm

clean:
	rm -rf $(BUILD)

.PHONY: all clean
