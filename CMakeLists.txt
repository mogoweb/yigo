cmake_minimum_required(VERSION 3.16)
project(YiGo VERSION 0.1.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_AUTOMOC ON)

find_package(Qt5 5.11 REQUIRED COMPONENTS Core Test Widgets)

add_subdirectory(src/core)
add_subdirectory(src/tools)
add_subdirectory(src/ui)
enable_testing()
add_subdirectory(tests)
add_subdirectory(tests/gui)
