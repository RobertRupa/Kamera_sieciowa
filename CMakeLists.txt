cmake_minimum_required(VERSION 3.22)

# The ESP-IDF project root is the repository root. Keep the existing modular
# sources under firmware/ without requiring VS Code to open that subdirectory.
set(EXTRA_COMPONENT_DIRS
    "${CMAKE_CURRENT_LIST_DIR}/firmware/main"
    "${CMAKE_CURRENT_LIST_DIR}/firmware/components"
    "${CMAKE_CURRENT_LIST_DIR}/templates/esp-idf/components/board_capabilities"
)

include($ENV{IDF_PATH}/tools/cmake/project.cmake)
project(camera_hub VERSION 0.1.0)

if(NOT IDF_TARGET STREQUAL "esp32p4")
    message(FATAL_ERROR "camera_hub requires IDF_TARGET=esp32p4")
endif()
