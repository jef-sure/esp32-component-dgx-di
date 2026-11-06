# DGX-DI built on a PC with make, beside DGX's host/dgx_host.mk: include this file from a Makefile.
#
#   include path/to/dgx/host/dgx_host.mk
#   include path/to/dgx-di/host/dgx_di_host.mk
#   CPPFLAGS += $(DGX_HOST_CPPFLAGS) $(DGX_HOST_WINDOW_CPPFLAGS) $(DGX_DI_HOST_CPPFLAGS)
#   CFLAGS   += $(DGX_HOST_CFLAGS)
#   LDLIBS   += $(DGX_HOST_LDLIBS) $(DGX_HOST_WINDOW_LDLIBS)
#   check: main.c $(DGX_HOST_SRCS) $(DGX_HOST_WINDOW_SRCS) $(DGX_DI_HOST_SRCS) $(DGX_DI_HOST_WINDOW_SRCS)
#
# DGX_DI_HOST_SRCS is every group below but the window's; take the groups one by one to build less. The paths are
# absolute, found from where this file lies. The touch of a window of DGX, the mouse over it, is dgx_di_host.h, all of
# it static inline: it needs DGX 0.10.10 and its group DGX_HOST_WINDOW. The GSL3680 is a chip on the I2C of a board:
# not here.

DGX_DI_HOST_DIR := $(patsubst %/,%,$(dir $(abspath $(lastword $(MAKEFILE_LIST)))))
DGX_DI_ROOT     := $(patsubst %/,%,$(dir $(DGX_DI_HOST_DIR)))

# the fingers of a device as fingers of the display, the gestures of a touch, rectangles to touch, a calibration made
DGX_DI_HOST_SRCS_CORE     := $(addprefix $(DGX_DI_ROOT)/src/, dgx_di_pointer.c dgx_di_touch.c dgx_di_rects.c dgx_di_calibration.c)
# the pad in a layer of DGX, and its ready layouts (DGX's virtual screens and overlay: DGX_HOST_SRCS_VSCREEN)
DGX_DI_HOST_SRCS_PAD      := $(addprefix $(DGX_DI_ROOT)/src/, dgx_di_pad.c dgx_di_pad_layouts.c)
# the keyboard on the display, and its ready layouts
DGX_DI_HOST_SRCS_KEYBOARD := $(addprefix $(DGX_DI_ROOT)/src/, dgx_di_keyboard.c dgx_di_keyboard_layouts.c)

DGX_DI_HOST_SRCS := $(DGX_DI_HOST_SRCS_CORE) $(DGX_DI_HOST_SRCS_PAD) $(DGX_DI_HOST_SRCS_KEYBOARD)

# the calibration measured on the display, target by target: it waits and keeps time, as only DGX's window does
DGX_DI_HOST_WINDOW_SRCS := $(DGX_DI_ROOT)/src/dgx_di_calibrate.c

DGX_DI_HOST_CPPFLAGS := -I$(DGX_DI_ROOT)/include
