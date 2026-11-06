# no REQUIRES: main takes every component, DGX-DI among them under the name of its checkout's directory
idf_component_register(SRCS "touch_check.c" "board_jc4827w543.c" "board_computer.c" INCLUDE_DIRS ".")

# the text of the check, about 3.4 mm high on any display: the board's pixels and millimetres say how many pixels; the
# computer's window is the JC4827W543's glass
if(CONFIG_TOUCH_CHECK_BOARD_JC4827W543 OR CONFIG_TOUCH_CHECK_BOARD_COMPUTER)
    dgx_fonts_board(PIXELS 480 272 MM 95 54)
endif()
dgx_font_needs_file(fonts.needs)
