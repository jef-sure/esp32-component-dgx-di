# touch_check on the computer with make: main/touch_check.c as it is, main/board_computer.c its board, a window of
# SDL2 in place of the JC4827W543's display (host/window of DGX) and the mouse over it as the finger (dgx_di_host.h).
# make, then make run (or ./build-host/touch_check); make clean. Needs SDL2 (sdl2-config), Python 3 and FreeType for
# DGX's fonts made at the build (font2c), and a shell without ESP-IDF activated: its linker does not link programs for
# the computer. DGX: the checkout beside DGX-DI's, else the one the component manager put into managed_components/ for
# the linux target; DGX=<dgx> takes another one, 0.10.10 or later.
DI    := ../..
DGX   ?= $(firstword $(wildcard $(DI)/../esp32-component-dgx managed_components/jef-sure__dgx))
BUILD := build-host
PROG  := $(BUILD)/touch_check
.DEFAULT_GOAL := $(PROG)

ifeq ($(wildcard $(DGX)/host/window/dgx_host_screen.h),)
$(error no DGX with a window at '$(DGX)': make DGX=<a checkout of DGX 0.10.10 or later>)
endif

include $(DGX)/host/dgx_host.mk
include $(DI)/host/dgx_di_host.mk

ifneq ($(findstring .espressif,$(shell command -v ld)),)
$(error ld is ESP-IDF's ($(shell command -v ld)): build for the computer in a shell without ESP-IDF activated)
endif

CFLAGS   ?= -O2 -g
CFLAGS   += $(DGX_HOST_CFLAGS) -MMD -MP
CPPFLAGS += $(DGX_HOST_CPPFLAGS) $(DGX_HOST_WINDOW_CPPFLAGS) $(DGX_DI_HOST_CPPFLAGS) -I$(BUILD)/fonts -DCONFIG_TOUCH_CHECK_BOARD_COMPUTER=1
LDLIBS   += $(DGX_HOST_LDLIBS) $(DGX_HOST_WINDOW_LDLIBS)

# the font of the check, as main/CMakeLists.txt asks for it, for the JC4827W543's pixels and millimetres: dgx_fonts.py
# rewrites its files only when they change, so a stamp says when it ran
FONTS := $(BUILD)/fonts
$(FONTS)/made: main/fonts.needs main/touch_check.c main/board_jc4827w543.c main/board_computer.c $(DGX)/font2c/dgx_fonts.py
	@mkdir -p $(FONTS)
	python3 $(DGX)/font2c/dgx_fonts.py build main/fonts.needs $(FONTS) --pixels 480 272 --mm 95 54
	@touch $@
# by both names: the objects name the source by its whole path, the dependencies of main/ the header by the build's
$(FONTS)/dgx_fonts.c $(FONTS)/dgx_fonts.h $(abspath $(FONTS))/dgx_fonts.c $(abspath $(FONTS))/dgx_fonts.h: $(FONTS)/made ;

# DGX without its own fonts (the check has its own); an object of every source under build-host/, where the source lies
DGX_SRCS := $(DGX_HOST_SRCS_SCREEN) $(DGX_HOST_SRCS_DRAW) $(DGX_HOST_SRCS_VSCREEN) $(DGX_HOST_SRCS_TEXT) $(DGX_HOST_SRCS_MORPH) \
            $(DGX_HOST_SRCS_FB) $(DGX_HOST_WINDOW_SRCS)
SRCS := $(abspath main/touch_check.c main/board_computer.c $(FONTS)/dgx_fonts.c) $(DGX_SRCS) \
        $(DGX_DI_HOST_SRCS) $(DGX_DI_HOST_WINDOW_SRCS)
OBJS := $(patsubst /%.c,$(BUILD)/obj/%.o,$(SRCS))

$(PROG): $(OBJS)
	$(CC) $(CFLAGS) $^ -o $@ $(LDLIBS)

# dgx_fonts.h made before anything is compiled: main/ includes it
$(BUILD)/obj/%.o: /%.c | $(FONTS)/made
	@mkdir -p $(@D)
	$(CC) $(CPPFLAGS) $(CFLAGS) -c $< -o $@

run: $(PROG)
	./$(PROG)

clean:
	rm -rf $(BUILD)

.PHONY: run clean

-include $(OBJS:.o=.d)
