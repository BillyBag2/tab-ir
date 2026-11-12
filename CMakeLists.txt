# The following five lines of boilerplate have to be in your project's
# CMakeLists in this exact order for cmake to work correctly
cmake_minimum_required(VERSION 3.22)

# Board libraries are pinned by tools/setup-dependencies.ps1 as local components.
set(ENV{IDF_COMPONENT_MANAGER} "0")
set(EXTRA_COMPONENT_DIRS
    "${CMAKE_CURRENT_LIST_DIR}/components/esp-usb/host/usb"
    "${CMAKE_CURRENT_LIST_DIR}/components/esp-usb/host/class/hid/usb_host_hid")
include($ENV{IDF_PATH}/tools/cmake/project.cmake)
project(tab_ir)
