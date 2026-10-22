# dgx_di_keyboard_font() of project_include.cmake run as a script, with DGX's dgx_font_need() a stub that writes what
# it was asked: cmake -D LAYOUT=<layout.json> -D NEED=<name> [-D FILE=<font.c>] -P keyboard_font.cmake, in a directory
# of its own (the letters go to dgx_di/<name>.letters there, the request to <name>.need)
include(${CMAKE_CURRENT_LIST_DIR}/../../project_include.cmake)
function(dgx_font_need)
    file(WRITE "${CMAKE_BINARY_DIR}/${NEED}.need" "${ARGN}")
endfunction()
if(FILE)
    dgx_di_keyboard_font(${NEED} "${LAYOUT}" FILE "${FILE}")
else()
    dgx_di_keyboard_font(${NEED} "${LAYOUT}")
endif()
