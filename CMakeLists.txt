cmake_minimum_required(VERSION 3.19)

project("sdb" LANGUAGES CXX ASM)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

# Ridiculously calls to add pie are ignored if we don't check it...
include(CheckPIESupported)
check_pie_supported()

include(CTest)

find_package(PkgConfig REQUIRED)
pkg_check_modules(readline REQUIRED IMPORTED_TARGET readline)

find_package(zydis CONFIG REQUIRED)

add_subdirectory("src")
add_subdirectory("tools")

if(BUILD_TESTING)
    find_package(Catch2 CONFIG REQUIRED)
    add_subdirectory("test")
endif()

