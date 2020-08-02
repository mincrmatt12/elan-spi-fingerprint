cmake_minimum_required(VERSION 3.9)
project(elanspiproto CXX)

# Find libudev
find_package(PkgConfig REQUIRED)

pkg_check_modules(UDEV REQUIRED IMPORTED_TARGET libudev)

add_executable(prototype main.cpp)

target_link_libraries(prototype PUBLIC PkgConfig::UDEV)
target_compile_options(prototype PUBLIC -g)
