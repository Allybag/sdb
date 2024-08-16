cmake_minimum_required(VERSION 3.19)

project("sdb" LANGUAGES CXX)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)


include(CTest)

find_package(PkgConfig REQUIRED)
pkg_check_modules(readline REQUIRED IMPORTED_TARGET readline)

add_subdirectory("src")
add_subdirectory("tools")

if(BUILD_TESTING)
    find_package(Catch2 CONFIG REQUIRED)
    add_subdirectory("test")
endif()

