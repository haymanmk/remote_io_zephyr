cmake_minimum_required(VERSION 3.20.0)

# Kconfig options

# Device tree overlay file
set(DTC_OVERLAY_FILE "${CMAKE_CURRENT_SOURCE_DIR}/boards/nucleo_f767zi_firmware.overlay")

# Declare project
find_package(Zephyr REQUIRED HINTS $ENV{ZEPHYR_BASE})
project(remote_io_zephyr)

# Sources
file(GLOB_RECURSE app_sources src/*.c)
target_sources(app PRIVATE ${app_sources})

# Generate Root CA include files
generate_inc_file_for_target(app "src/inc/AmazonRootCA1.cer" "${ZEPHYR_BINARY_DIR}/include/generated/AmazonRootCA1.cer.inc")
generate_inc_file_for_target(app "src/inc/gts-root-r4.pem" "${ZEPHYR_BINARY_DIR}/include/generated/gts-root-r4.pem.inc")
