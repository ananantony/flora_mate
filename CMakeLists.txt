cmake_minimum_required(VERSION 3.22)

#
# This file is generated only once,
# and is not re-generated if converter is called multiple times.
#
# User is free to modify the file as much as necessary
#

# Setup compiler settings
set(CMAKE_C_STANDARD 11)
set(CMAKE_C_STANDARD_REQUIRED ON)
set(CMAKE_C_EXTENSIONS ON)


# Define the build type
if(NOT CMAKE_BUILD_TYPE)
    set(CMAKE_BUILD_TYPE "Debug")
endif()

# Set the project name
set(CMAKE_PROJECT_NAME FloraMate)

# Enable compile command to ease indexing with e.g. clangd
set(CMAKE_EXPORT_COMPILE_COMMANDS TRUE)

# Core project settings
project(${CMAKE_PROJECT_NAME})
message("Build type: " ${CMAKE_BUILD_TYPE})

# Enable CMake support for ASM and C languages
enable_language(C ASM)

# Create an executable object type
add_executable(${CMAKE_PROJECT_NAME})

# Add STM32CubeMX generated sources
add_subdirectory(cmake/stm32cubemx)

# Link directories setup
target_link_directories(${CMAKE_PROJECT_NAME} PRIVATE
    # Add user defined library search paths
)

# User application sources (App / Bsp / Common)
file(GLOB_RECURSE FM_USER_SOURCES CONFIGURE_DEPENDS
    ${CMAKE_CURRENT_SOURCE_DIR}/App/*.c
    ${CMAKE_CURRENT_SOURCE_DIR}/Bsp/*.c
    ${CMAKE_CURRENT_SOURCE_DIR}/Common/*.c
)

# Add sources to executable
target_sources(${CMAKE_PROJECT_NAME} PRIVATE
    # Add user sources here
    ${FM_USER_SOURCES}
)

# Add include paths
target_include_directories(${CMAKE_PROJECT_NAME} PRIVATE
    # Add user defined include paths
    # App
    ${CMAKE_CURRENT_SOURCE_DIR}/App/config
    ${CMAKE_CURRENT_SOURCE_DIR}/App/display
    ${CMAKE_CURRENT_SOURCE_DIR}/App/event
    ${CMAKE_CURRENT_SOURCE_DIR}/App/init
    ${CMAKE_CURRENT_SOURCE_DIR}/App/log
    ${CMAKE_CURRENT_SOURCE_DIR}/App/main_fsm
    ${CMAKE_CURRENT_SOURCE_DIR}/App/manual_key
    ${CMAKE_CURRENT_SOURCE_DIR}/App/manual_select
    ${CMAKE_CURRENT_SOURCE_DIR}/App/menu
    ${CMAKE_CURRENT_SOURCE_DIR}/App/pump_fsm
    ${CMAKE_CURRENT_SOURCE_DIR}/App/serial_debug
    # Bsp
    ${CMAKE_CURRENT_SOURCE_DIR}/Bsp/eeprom
    ${CMAKE_CURRENT_SOURCE_DIR}/Bsp/key
    ${CMAKE_CURRENT_SOURCE_DIR}/Bsp/oled
    ${CMAKE_CURRENT_SOURCE_DIR}/Bsp/pump_pwm
    ${CMAKE_CURRENT_SOURCE_DIR}/Bsp/relay
    ${CMAKE_CURRENT_SOURCE_DIR}/Bsp/tick
    ${CMAKE_CURRENT_SOURCE_DIR}/Bsp/usart
    ${CMAKE_CURRENT_SOURCE_DIR}/Bsp/valve
    # Common
    ${CMAKE_CURRENT_SOURCE_DIR}/Common/crc16
    ${CMAKE_CURRENT_SOURCE_DIR}/Common/ring_buffer
    ${CMAKE_CURRENT_SOURCE_DIR}/Common/types
)

# Add project symbols (macros)
target_compile_definitions(${CMAKE_PROJECT_NAME} PRIVATE
    # Add user defined symbols
)

# Remove wrong libob.a library dependency when using cpp files
list(REMOVE_ITEM CMAKE_C_IMPLICIT_LINK_LIBRARIES ob)

# Add linked libraries
target_link_libraries(${CMAKE_PROJECT_NAME}
    stm32cubemx

    # Add user defined libraries
)
