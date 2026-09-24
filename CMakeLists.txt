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

# Debian packaging: builds a .deb from the already-built binary
find_program(DPKG_DEB_EXECUTABLE dpkg-deb)
if(DPKG_DEB_EXECUTABLE)
    add_custom_target(deb
        COMMAND ${CMAKE_COMMAND} -E make_directory ${CMAKE_BINARY_DIR}/debroot/DEBIAN
        COMMAND ${CMAKE_COMMAND} -E make_directory ${CMAKE_BINARY_DIR}/debroot/usr/bin
        COMMAND ${CMAKE_COMMAND} -E make_directory ${CMAKE_BINARY_DIR}/debroot/usr/share/applications
        COMMAND ${CMAKE_COMMAND} -E make_directory ${CMAKE_BINARY_DIR}/debroot/usr/share/icons/hicolor/scalable/apps
        COMMAND ${CMAKE_COMMAND} -E copy ${CMAKE_BINARY_DIR}/src/ui/yigo ${CMAKE_BINARY_DIR}/debroot/usr/bin/yigo
        COMMAND strip --strip-unneeded ${CMAKE_BINARY_DIR}/debroot/usr/bin/yigo
        COMMAND ${CMAKE_COMMAND} -E copy ${CMAKE_SOURCE_DIR}/packaging/debian/yigo.desktop ${CMAKE_BINARY_DIR}/debroot/usr/share/applications/yigo.desktop
        COMMAND ${CMAKE_COMMAND} -E copy ${CMAKE_SOURCE_DIR}/packaging/icon.svg ${CMAKE_BINARY_DIR}/debroot/usr/share/icons/hicolor/scalable/apps/yigo.svg
        COMMAND ${CMAKE_COMMAND} -E copy ${CMAKE_SOURCE_DIR}/packaging/debian/control ${CMAKE_BINARY_DIR}/debroot/DEBIAN/control
        COMMAND ${DPKG_DEB_EXECUTABLE} --build --root-owner-group
                ${CMAKE_BINARY_DIR}/debroot ${CMAKE_BINARY_DIR}/yigo_0.5.0_amd64.deb
        DEPENDS yigo
        COMMENT "Building yigo_0.5.0_amd64.deb"
        VERBATIM
    )
endif()
