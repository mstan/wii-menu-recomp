# SPDX-License-Identifier: GPL-3.0-or-later
set(WIILLR_MENU_ROOT "${CMAKE_CURRENT_LIST_DIR}")
if(NOT WIILLR_MM9_ROOT)
  set(WIILLR_MM9_ROOT "${WIILLR_MENU_ROOT}/external/MegaMan9Recomp")
endif()
if(NOT EXISTS "${WIILLR_MM9_ROOT}/components.cmake")
  message(FATAL_ERROR "Initialize the pinned Mega Man 9 title provider")
endif()
set(WIILLR_MENU_PICTURES declared-slice CACHE STRING "Menu material provider")
set(WIILLR_MENU_TEXT strict CACHE STRING "Menu text provider")
set(WIILLR_MENU_FRAME native CACHE STRING "Menu frame provider")
set(WIILLR_MENU_TRANSFORM ON CACHE BOOL "Menu transforms")
set(WIILLR_MM9_RENDERER declared-slice CACHE STRING "MM9 renderer")
set(WIILLR_MM9_FRAME native CACHE STRING "MM9 frame provider")
set(WIILLR_PRODUCT_CATALOG "${WIILLR_MENU_ROOT}/catalog-all.cmake")
set(WIILLR_NATIVE_POLICY "${WIILLR_MENU_ROOT}/src/native_policy.inc")
set(WIILLR_LAUNCHER_POLICY "${WIILLR_MENU_ROOT}/src/launcher_policy.inc")
set(WIILLR_PRODUCT_LAUNCHER_SOURCES "${WIILLR_MM9_ROOT}/src/launcher_mods.c")
set(WIILLR_PRODUCT_TESTS "${WIILLR_MENU_ROOT}/tests.cmake")
macro(wiillr_product_components)
  include("${WIILLR_MENU_ROOT}/components.cmake")
  include("${WIILLR_MM9_ROOT}/components.cmake")
endmacro()

set_property(SOURCE src/launcher.c "${WIILLR_MENU_ROOT}/tests/test_launcher_window.c" APPEND PROPERTY COMPILE_DEFINITIONS "WIILLR_LAUNCHER_POLICY=\"${WIILLR_LAUNCHER_POLICY}\"" "WIILLR_MM9_LAUNCHER_TITLE=\"${WIILLR_MM9_ROOT}/src/launcher_title.inc\"")

set_property(SOURCE "${WIILLR_MENU_ROOT}/tests/test_launcher_window.c" APPEND PROPERTY COMPILE_DEFINITIONS "WIILLR_ENGINE_LAUNCHER_SOURCE=\"${CMAKE_CURRENT_SOURCE_DIR}/src/launcher.c\"")

set_property(SOURCE src/native.c tests/test_native.c APPEND PROPERTY COMPILE_DEFINITIONS "WIILLR_MM9_NATIVE_TITLE=\"${WIILLR_MM9_ROOT}/src/native_title.inc\"")

set(_menu_codec_sources native/menu_picture.cpp native/menu_text.cpp native/menu_text_draw.cpp
  "${WIILLR_MENU_ROOT}/src/menu_picture_hook.c" "${WIILLR_MENU_ROOT}/src/menu_text_hook.c")
file(GLOB _menu_test_sources "${WIILLR_MENU_ROOT}/tests/*.cpp" "${WIILLR_MENU_ROOT}/tests/*.c")
set_property(SOURCE ${_menu_codec_sources} ${_menu_test_sources} APPEND PROPERTY COMPILE_DEFINITIONS
  "WIILLR_NW4R_DEFAULT_CLASSES=0x816717f4,0x81671a20,0x81671b48,0x8167192c,0x8166fe88,0x8166ff70"
  WIILLR_NW4R_SDK_POINTER=0x81695618)
