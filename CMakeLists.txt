cmake_minimum_required(VERSION 3.20.0)

# Kconfig options
set(KCONFIG_ROOT "${CMAKE_CURRENT_SOURCE_DIR}/Kconfig")

# Device tree overlay file
set(DTC_OVERLAY_FILE "${CMAKE_CURRENT_SOURCE_DIR}/boards/nucleo_f767zi_firmware.overlay")

# Declare project
find_package(Zephyr REQUIRED HINTS $ENV{ZEPHYR_BASE})
project(remote_io_zephyr)

target_include_directories(app PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/src/inc)

# Sources
zephyr_library_sources_ifdef(CONFIG_REMOTEIO_USE_MY_WS28XX
    src/ws28xx_pwm.c)
zephyr_library_sources_ifndef(CONFIG_REMOTEIO_USE_MY_WS28XX
    src/ws28xx_led.c)
# file(GLOB_RECURSE app_sources src/*.c)
file(GLOB_RECURSE app_sources
    src/uart.c
    src/flash.c
    src/settings.c
    src/digital_input.c
    src/digital_output.c
    src/utils.c
    src/api.c
    src/ethernet_if.c
    src/system_info.c
    src/main.c)
target_sources(app PRIVATE ${app_sources})

# Generate Root CA include files
generate_inc_file_for_target(app "src/inc/AmazonRootCA1.cer" "${ZEPHYR_BINARY_DIR}/include/generated/AmazonRootCA1.cer.inc")
generate_inc_file_for_target(app "src/inc/gts-root-r4.pem" "${ZEPHYR_BINARY_DIR}/include/generated/gts-root-r4.pem.inc")
