# Owl firmware - application CMakeLists.txt
# Builds the Zephyr application targeting RP2350 (Pico 2)

cmake_minimum_required(VERSION 3.20.0)

find_package(Zephyr REQUIRED HINTS $ENV{ZEPHYR_BASE})
project(owl_firmware)

target_sources(app PRIVATE src/main.c)
