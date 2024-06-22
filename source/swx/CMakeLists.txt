cmake_minimum_required(VERSION 3.26)

if (NOT PICO_BOARD)
set(PICO_BOARD SW32)
endif()

if (NOT CMAKE_BUILD_TYPE)
set(CMAKE_BUILD_TYPE Release)
endif()

set(CMAKE_C_STANDARD 11)
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

# Add the board directory so pico-sdk can find it
list(APPEND PICO_BOARD_HEADER_DIRS ${CMAKE_SOURCE_DIR}/boards)

# Pull in Pico SDK
include(pico_sdk_import.cmake)

# Pull in FreeRTOS
set(FREERTOS_KERNEL_PATH "${CMAKE_CURRENT_LIST_DIR}/lib/freertos")
include(FreeRTOS_Kernel_import.cmake)

# Pull in libfixmath
add_subdirectory(lib/libfixmath)

# Pull in Lua
add_subdirectory(lib/lua)

project(swx
    VERSION 1.0.0 # pcb_rev.major.minor
    HOMEPAGE_URL https://github.com/saawsm/SW32
    LANGUAGES C CXX ASM
)

# Init the Raspberry Pi Pico SDK
pico_sdk_init()

add_executable(${PROJECT_NAME})
pico_add_extra_outputs(${PROJECT_NAME}) # Create map/bin/hex/uf2 files

target_sources(${PROJECT_NAME} PRIVATE
    "src/main.c"
    "src/util/i2c.c"
)

target_include_directories(${PROJECT_NAME} PRIVATE
    "config"
    "include/swx"
)

target_link_libraries(${PROJECT_NAME} PRIVATE 
    pico_stdlib
    FreeRTOS-Kernel
    FreeRTOS-Kernel-Heap4
    libfixmath
    lua

    hardware_i2c
    hardware_uart
    hardware_pio
    hardware_adc
    hardware_dma
)

target_compile_options(${PROJECT_NAME} PRIVATE
    -Wall
    -Wextra
    -Wno-format # int != int32_t as far as the compiler is concerned because gcc has int32_t as long int
)

target_compile_definitions(${PROJECT_NAME} PRIVATE
    FIXMATH_FAST_SIN
    PICO_USE_STACK_GUARDS
)

pico_enable_stdio_usb(${PROJECT_NAME} 1)
pico_enable_stdio_uart(${PROJECT_NAME} 0)

pico_set_program_name(${PROJECT_NAME} ${PROJECT_NAME})
pico_set_program_version(${PROJECT_NAME} ${PROJECT_VERSION})

if(CMAKE_BUILD_TYPE STREQUAL "Debug")
    target_compile_definitions(${PROJECT_NAME} PRIVATE PICO_STDIO_USB_CONNECT_WAIT_TIMEOUT_MS=5000)    
endif()