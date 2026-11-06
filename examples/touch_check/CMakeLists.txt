# touch_check: the touch panel of a board measured and tried on the board itself (main/touch_check.c), the board
# picked in menuconfig (main/Kconfig.projbuild), its display and panel set up by its own file (main/board_*.c). On the
# computer too: for the linux target of ESP-IDF here, with make by the Makefile beside main/ (main/board_computer.c).
cmake_minimum_required(VERSION 3.16)
# DGX-DI as it is in this checkout, whatever its directory is named
set(EXTRA_COMPONENT_DIRS ../..)
# the linux target of ESP-IDF: only main and what it needs, as not every component of ESP-IDF builds for that target;
# DGX-DI by the name of its checkout's directory, as main names no component
if(IDF_TARGET STREQUAL "linux")
    get_filename_component(dgx_di_name ../.. ABSOLUTE)
    get_filename_component(dgx_di_name "${dgx_di_name}" NAME)
    set(COMPONENTS main ${dgx_di_name})
endif()
include($ENV{IDF_PATH}/tools/cmake/project.cmake)
project(touch_check)
