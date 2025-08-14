# == DO NOT EDIT THE FOLLOWING LINES for the Raspberry Pi Pico VS Code Extension to work ==
if(WIN32)
    set(USERHOME $ENV{USERPROFILE})
else()
    set(USERHOME $ENV{HOME})
endif()
set(sdkVersion 2.2.0)
set(toolchainVersion 14_2_Rel1)
set(picotoolVersion 2.2.0)
set(picoVscode ${USERHOME}/.pico-sdk/cmake/pico-vscode.cmake)
if (EXISTS ${picoVscode})
    include(${picoVscode})
endif()
# ====================================================================================
set(PICO_BOARD pico CACHE STRING "Board type")

cmake_minimum_required(VERSION 3.17.0)

include(pico_sdk_import.cmake)

project(MSRC-RP2040)

include_directories(../include)

execute_process(
    COMMAND git config --global --add safe.directory /github/workspace
    COMMAND git describe --tags
    OUTPUT_VARIABLE PROJECT_VERSION
    OUTPUT_STRIP_TRAILING_WHITESPACE
)
message(STATUS "MSRC ${PROJECT_VERSION}") 
add_compile_definitions(PROJECT_VERSION="${PROJECT_VERSION}")

pico_sdk_init()

add_subdirectory(freertos)
add_subdirectory(project)
