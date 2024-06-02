cmake_minimum_required(VERSION 3.26)

# Development / debugging options
set(USB_WAIT_TIME_MS 0 CACHE STRING "Delay in milliseconds to wait for USB connection (Defaults OFF, or 15 seconds in Debug builds)")

set(CMAKE_C_STANDARD 11)
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

# Add the board directory so the pico-sdk can find it
list(APPEND PICO_BOARD_HEADER_DIRS ${CMAKE_SOURCE_DIR}/boards)

# Pull in Pico SDK (must be before project) (environment variable PICO_SDK_PATH)
include($ENV{PICO_SDK_PATH}/pico_sdk_init.cmake)

project(swx
    VERSION 0.1
    HOMEPAGE_URL https://github.com/saawsm/SW32
    LANGUAGES C CXX ASM
)

# Init the Raspberry Pi Pico SDK
pico_sdk_init()

add_executable(${PROJECT_NAME})

# Add source files.
target_sources(${PROJECT_NAME}
    PRIVATE
        "src/main.cpp"
)

# Link the pico standard library 
target_link_libraries(${PROJECT_NAME} PRIVATE pico_stdlib)

# Link libraries
target_link_libraries(${PROJECT_NAME}
    PRIVATE
        hardware_i2c
        hardware_pio
        hardware_adc
        hardware_dma
        pico_multicore
)

target_compile_options(${PROJECT_NAME} PRIVATE
    -Wall
    -Wextra
    -Wno-format # int != int32_t as far as the compiler is concerned because gcc has int32_t as long int
)

pico_set_program_name(${PROJECT_NAME} ${PROJECT_NAME})
pico_set_program_version(${PROJECT_NAME} ${PROJECT_VERSION})

pico_enable_stdio_usb(${PROJECT_NAME} 1)
pico_enable_stdio_uart(${PROJECT_NAME} 0)

# pico_set_float_implementation(${PROJECT_NAME} pico)
# pico_set_binary_type(${PROJECT_NAME} no_flash)

# Create map/bin/hex/uf2 files
pico_add_extra_outputs(${PROJECT_NAME})

# -----------------------------------------------------------------------------------------------------------------

if(CMAKE_BUILD_TYPE STREQUAL "Debug" AND NOT USB_WAIT_TIME_MS)
    message("Set USB_WAIT_TIME_MS to 15 seconds, since debug")
    set (USB_WAIT_TIME_MS 15000)
endif()

if(USB_WAIT_TIME_MS)
    target_compile_definitions(${PROJECT_NAME} PRIVATE PICO_STDIO_USB_CONNECT_WAIT_TIMEOUT_MS=${USB_WAIT_TIME_MS})
endif()