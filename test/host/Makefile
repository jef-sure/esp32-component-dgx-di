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

# with Python, the ready layouts of the keyboard made again of their JSON and compared; with Node, the geometry of the
# keyboard's editor compared with the C's
TOOLS := $(if $(shell command -v python3),check-layouts check-pad-layouts) $(if $(shell command -v node),check-keyboard-js check-pad-js) \
	$(if $(and $(shell command -v python3),$(shell command -v node)),check-templates)

all: run-test_pointer run-test_touch run-test_calibration run-test_rects run-test_pad run-test_keyboard $(TOOLS)

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

$(BUILD)/test_pad: test_pad.c check.h $(ROOT)/src/dgx_di_pad.c $(ROOT)/src/dgx_di_pad_layouts.c $(ROOT)/src/dgx_di_pointer.c $(ROOT)/include/dgx_di.h $(ROOT)/include/dgx_di_pad.h
	mkdir -p $(BUILD)
	$(CC) $(CFLAGS) $(DGX_FLAGS) -o $@ test_pad.c $(ROOT)/src/dgx_di_pad.c $(ROOT)/src/dgx_di_pad_layouts.c $(ROOT)/src/dgx_di_pointer.c $(DGX_SRCS) -lm

KEYBOARD := $(ROOT)/src/dgx_di_keyboard.c $(ROOT)/src/dgx_di_keyboard_layouts.c $(ROOT)/src/dgx_di_rects.c $(ROOT)/src/dgx_di_pointer.c

$(BUILD)/test_keyboard: test_keyboard.c check.h $(KEYBOARD) $(ROOT)/include/dgx_di.h $(ROOT)/include/dgx_di_keyboard.h
	mkdir -p $(BUILD)
	$(CC) $(CFLAGS) $(DGX_FLAGS) -o $@ test_keyboard.c $(KEYBOARD) $(DGX_SRCS) -lm

# includes the keyboard's source (its lay() of each page): the rest of it only
$(BUILD)/keyboard_dump: keyboard_dump.c $(KEYBOARD) $(ROOT)/include/dgx_di_keyboard.h
	mkdir -p $(BUILD)
	$(CC) $(CFLAGS) $(DGX_FLAGS) -o $@ keyboard_dump.c $(filter-out %/dgx_di_keyboard.c,$(KEYBOARD)) $(DGX_SRCS) -lm

check-layouts:
	mkdir -p $(BUILD)
	python3 $(ROOT)/tools/keyboard_blob.py --builtin $(BUILD)/dgx_di_keyboard_layouts.c $(ROOT)/tools/keyboard-editor/examples/*.json
	cmp $(BUILD)/dgx_di_keyboard_layouts.c $(ROOT)/src/dgx_di_keyboard_layouts.c && echo "the ready layouts: as their JSON makes them"

# text drawn by DGX in some of its own fonts, and the Latin keyboard's labels in one: the editor's labels compared
DGX_FONTS := $(addprefix $(DGX)/src/fonts/, ArialRegular12.c TerminusTTFMedium12.c VerdanaRegular32.c)
$(BUILD)/font_dump: font_dump.c $(KEYBOARD) $(ROOT)/include/dgx_di_keyboard.h
	mkdir -p $(BUILD)
	$(CC) $(CFLAGS) $(DGX_FLAGS) -o $@ font_dump.c $(KEYBOARD) $(DGX_FONTS) $(DGX_SRCS) -lm

check-keyboard-js: $(BUILD)/keyboard_dump $(BUILD)/font_dump
	./$(BUILD)/keyboard_dump > $(BUILD)/keyboard-geometry.json
	./$(BUILD)/font_dump > $(BUILD)/font-dump.json
	mkdir -p $(BUILD)/keyboard-templates
	node $(ROOT)/tools/keyboard-editor/test.js $(BUILD)/keyboard-geometry.json $(BUILD)/keyboard-templates $(BUILD)/font-dump.json $(DGX)/src/fonts

check-pad-layouts:
	mkdir -p $(BUILD)
	python3 $(ROOT)/tools/pad_blob.py --builtin $(BUILD)/dgx_di_pad_layouts.c $(ROOT)/tools/pad-editor/examples/*.json
	cmp $(BUILD)/dgx_di_pad_layouts.c $(ROOT)/src/dgx_di_pad_layouts.c && echo "the ready pads: as their JSON makes them"

$(BUILD)/pad_dump: pad_dump.c $(ROOT)/src/dgx_di_pad.c $(ROOT)/src/dgx_di_pad_layouts.c $(ROOT)/src/dgx_di_pointer.c $(ROOT)/include/dgx_di_pad.h
	mkdir -p $(BUILD)
	$(CC) $(CFLAGS) $(DGX_FLAGS) -o $@ pad_dump.c $(ROOT)/src/dgx_di_pad.c $(ROOT)/src/dgx_di_pad_layouts.c $(ROOT)/src/dgx_di_pointer.c $(DGX_SRCS) -lm

check-pad-js: $(BUILD)/pad_dump
	./$(BUILD)/pad_dump > $(BUILD)/pad-geometry.json
	mkdir -p $(BUILD)/pad-templates
	node $(ROOT)/tools/pad-editor/test.js $(BUILD)/pad-geometry.json $(BUILD)/pad-templates

# the templates of the editors, as they write them: the tools make blobs of them
check-templates: check-keyboard-js check-pad-js
	python3 $(ROOT)/tools/keyboard_blob.py --builtin $(BUILD)/keyboard-templates.c $(BUILD)/keyboard-templates/*.json
	for f in $(BUILD)/keyboard-templates/*.json; do python3 $(ROOT)/tools/keyboard_blob.py --letters $${f%.json}.py-letters $$f && cmp $${f%.json}.py-letters $${f%.json}.letters || exit 1; done
	python3 $(ROOT)/tools/pad_blob.py --builtin $(BUILD)/pad-templates.c $(BUILD)/pad-templates/*.json
	@echo "the templates of the editors: the tools make blobs of them, and the keyboard's letters as the editor has them"

clean:
	rm -rf $(BUILD)

.PHONY: all clean check-layouts check-keyboard-js check-pad-layouts check-pad-js check-templates
