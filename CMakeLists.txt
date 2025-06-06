cmake_minimum_required(VERSION 3.20.0)

# Kconfig options
set(KCONFIG_ROOT "${CMAKE_CURRENT_SOURCE_DIR}/Kconfig")

# Device tree overlay file
set(DTC_OVERLAY_FILE "${CMAKE_CURRENT_SOURCE_DIR}/boards/nucleo_f767zi_firmware.overlay"
    "${CMAKE_CURRENT_SOURCE_DIR}/boards/nucleo_f767zi_ws2812_pwm.overlay")

# Declare project
find_package(Zephyr REQUIRED HINTS $ENV{ZEPHYR_BASE})
project(remote_io_zephyr)

target_include_directories(app PRIVATE 
    ${CMAKE_CURRENT_SOURCE_DIR}/src/inc
    ${CMAKE_CURRENT_SOURCE_DIR}/src/mender
    )

# Sources
zephyr_library_sources_ifdef(CONFIG_REMOTEIO_USE_MY_WS28XX
    src/ws28xx_pwm.c)
zephyr_library_sources_ifndef(CONFIG_REMOTEIO_USE_MY_WS28XX
    src/ws28xx_led.c)
file(GLOB_RECURSE app_sources src/*.c)
target_sources(app PRIVATE ${app_sources})

# Generate Root CA include files
generate_inc_file_for_target(app "src/inc/AmazonRootCA1.cer" "${ZEPHYR_BINARY_DIR}/include/generated/AmazonRootCA1.cer.inc")
generate_inc_file_for_target(app "src/inc/gts-root-r4.pem" "${ZEPHYR_BINARY_DIR}/include/generated/gts-root-r4.pem.inc")

if (CONFIG_MENDER_MCU_CLIENT)
    zephyr_library_compile_definitions(-D_POSIX_C_SOURCE=200809L)  # Required for strdup and strtok_r support
endif() # CONFIG_MENDER_MCU_CLIENT