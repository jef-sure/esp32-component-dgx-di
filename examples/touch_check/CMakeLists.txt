# touch_check: the touch panel of a board measured and tried on the board itself (main/touch_check.c), the board
# picked in menuconfig (main/Kconfig.projbuild), its display and panel set up by its own file (main/board_*.c).
cmake_minimum_required(VERSION 3.16)
# DGX-DI as it is in this checkout, whatever its directory is named
set(EXTRA_COMPONENT_DIRS ../..)
include($ENV{IDF_PATH}/tools/cmake/project.cmake)
project(touch_check)
