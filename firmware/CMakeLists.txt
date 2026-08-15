# Owl firmware - top-level CMakeLists.txt (west module)
# This file exists so the firmware/ directory is recognized as a west
# module containing Zephyr applications.

cmake_minimum_required(VERSION 3.20.0)

find_package(Zephyr REQUIRED HINTS $ENV{ZEPHYR_BASE})

add_subdirectory(app)
