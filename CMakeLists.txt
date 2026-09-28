cmake_minimum_required(VERSION 3.16)
project(YiGo VERSION 0.1.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_AUTOMOC ON)

find_package(Qt5 5.11 REQUIRED COMPONENTS Core Test Widgets)

# translations: compile .ts -> .qm at build time
# Locate lrelease next to the qmake of the Qt found above. Never fall back to
# PATH lookup: /usr/bin/lrelease is a qtchooser wrapper that dies with
# "could not find a Qt installation" when no default Qt is selected.
unset(LRELEASE_EXECUTABLE CACHE)   # drop stale entries from earlier configures
get_target_property(QT_QMAKE_EXECUTABLE Qt5::qmake IMPORTED_LOCATION)
if(QT_QMAKE_EXECUTABLE)
    get_filename_component(QT_BIN_DIR "${QT_QMAKE_EXECUTABLE}" DIRECTORY)
endif()
find_program(LRELEASE_EXECUTABLE NAMES lrelease
    HINTS ${QT_BIN_DIR}
    PATHS /usr/lib/qt5/bin
    NO_DEFAULT_PATH)
if(LRELEASE_EXECUTABLE)
    add_custom_command(
        OUTPUT ${CMAKE_BINARY_DIR}/yigo_zh_CN.qm
        COMMAND ${LRELEASE_EXECUTABLE}
            ${CMAKE_SOURCE_DIR}/translations/yigo_zh_CN.ts
            -qm ${CMAKE_BINARY_DIR}/yigo_zh_CN.qm
        DEPENDS ${CMAKE_SOURCE_DIR}/translations/yigo_zh_CN.ts
        COMMENT "Compiling yigo_zh_CN translation"
        VERBATIM
    )
    add_custom_target(translations ALL DEPENDS ${CMAKE_BINARY_DIR}/yigo_zh_CN.qm)
endif()

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
        COMMAND ${CMAKE_COMMAND} -E make_directory ${CMAKE_BINARY_DIR}/debroot/usr/share/yigo/translations
        COMMAND ${CMAKE_COMMAND} -E copy ${CMAKE_BINARY_DIR}/src/ui/yigo ${CMAKE_BINARY_DIR}/debroot/usr/bin/yigo
        COMMAND strip --strip-unneeded ${CMAKE_BINARY_DIR}/debroot/usr/bin/yigo
        COMMAND ${CMAKE_COMMAND} -E copy ${CMAKE_SOURCE_DIR}/packaging/debian/yigo.desktop ${CMAKE_BINARY_DIR}/debroot/usr/share/applications/yigo.desktop
        COMMAND ${CMAKE_COMMAND} -E copy ${CMAKE_SOURCE_DIR}/packaging/icon.svg ${CMAKE_BINARY_DIR}/debroot/usr/share/icons/hicolor/scalable/apps/yigo.svg
        COMMAND ${CMAKE_COMMAND} -E copy ${CMAKE_SOURCE_DIR}/packaging/debian/control ${CMAKE_BINARY_DIR}/debroot/DEBIAN/control
        COMMAND ${DPKG_DEB_EXECUTABLE} --build --root-owner-group
                ${CMAKE_BINARY_DIR}/debroot ${CMAKE_BINARY_DIR}/yigo_0.5.0_amd64.deb
        COMMAND ${CMAKE_COMMAND} -E copy ${CMAKE_BINARY_DIR}/yigo_zh_CN.qm ${CMAKE_BINARY_DIR}/debroot/usr/share/yigo/translations/yigo_zh_CN.qm
        DEPENDS yigo translations
        COMMENT "Building yigo_0.5.0_amd64.deb"
        VERBATIM
    )
endif()
