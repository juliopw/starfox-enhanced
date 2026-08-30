cmake_minimum_required(VERSION 3.24)

if(NOT DEFINED ENV{DEVKITPRO})
    message(FATAL_ERROR "DEVKITPRO is not set")
endif()

include("$ENV{DEVKITPRO}/cmake/Switch.cmake")

get_filename_component(STARFOX_ROOT
    "${CMAKE_CURRENT_LIST_DIR}/../.." ABSOLUTE)
set(NX_ROOT "${STARFOX_ROOT}/third_party/switch/libnx/nx")
set(NX_DEFAULT_ICON "${NX_ROOT}/default_icon.jpg")
