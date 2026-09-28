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

# Target architecture: picks the bundled engine build in engines/KataGo/<arch>
# and names the .deb. Uses the distro names shipped by UOS/Kylin debs.
# Override for cross builds: cmake -DYIGO_ENGINE_ARCH=arm64
if(NOT YIGO_ENGINE_ARCH)
    if(CMAKE_SYSTEM_PROCESSOR STREQUAL "x86_64")
        set(_detected_arch amd64)
    elseif(CMAKE_SYSTEM_PROCESSOR MATCHES "^aarch64")
        set(_detected_arch arm64)
    elseif(CMAKE_SYSTEM_PROCESSOR MATCHES "^loongarch64")
        set(_detected_arch loong64)
    elseif(CMAKE_SYSTEM_PROCESSOR MATCHES "^mips64")
        set(_detected_arch mips64el)
    else()
        string(TOLOWER "${CMAKE_SYSTEM_PROCESSOR}" _detected_arch)
    endif()
    set(YIGO_ENGINE_ARCH "${_detected_arch}" CACHE STRING
        "Target architecture for the bundled engine and the .deb name")
endif()
message(STATUS "Target architecture: ${YIGO_ENGINE_ARCH}")

add_subdirectory(src/core)
add_subdirectory(src/tools)
add_subdirectory(src/ui)
enable_testing()
add_subdirectory(tests)
add_subdirectory(tests/gui)

# Debian packaging: builds a .deb from the already-built binary
find_program(DPKG_DEB_EXECUTABLE dpkg-deb)
if(DPKG_DEB_EXECUTABLE)
    set(YIGO_DEB_NAME yigo_0.5.0_${YIGO_ENGINE_ARCH}.deb)
    configure_file(${CMAKE_SOURCE_DIR}/packaging/debian/control.in
                   ${CMAKE_BINARY_DIR}/packaging/debian/control @ONLY)
    add_custom_target(deb
        COMMAND ${CMAKE_COMMAND} -E make_directory ${CMAKE_BINARY_DIR}/debroot/DEBIAN
        COMMAND ${CMAKE_COMMAND} -E make_directory ${CMAKE_BINARY_DIR}/debroot/usr/bin
        COMMAND ${CMAKE_COMMAND} -E make_directory ${CMAKE_BINARY_DIR}/debroot/usr/share/applications
        COMMAND ${CMAKE_COMMAND} -E make_directory ${CMAKE_BINARY_DIR}/debroot/usr/share/icons/hicolor/scalable/apps
        COMMAND ${CMAKE_COMMAND} -E make_directory ${CMAKE_BINARY_DIR}/debroot/usr/share/yigo/translations
        COMMAND ${CMAKE_COMMAND} -E make_directory ${CMAKE_BINARY_DIR}/debroot/usr/share/yigo/engines/${YIGO_ENGINE_ARCH}
        COMMAND ${CMAKE_COMMAND} -E make_directory ${CMAKE_BINARY_DIR}/debroot/usr/share/yigo/engines/models
        COMMAND ${CMAKE_COMMAND} -E copy ${CMAKE_BINARY_DIR}/src/ui/yigo ${CMAKE_BINARY_DIR}/debroot/usr/bin/yigo
        COMMAND strip --strip-unneeded ${CMAKE_BINARY_DIR}/debroot/usr/bin/yigo
        COMMAND ${CMAKE_COMMAND} -E copy ${CMAKE_SOURCE_DIR}/packaging/debian/yigo.desktop ${CMAKE_BINARY_DIR}/debroot/usr/share/applications/yigo.desktop
        COMMAND ${CMAKE_COMMAND} -E copy ${CMAKE_SOURCE_DIR}/packaging/icon.svg ${CMAKE_BINARY_DIR}/debroot/usr/share/icons/hicolor/scalable/apps/yigo.svg
        COMMAND ${CMAKE_COMMAND} -E copy ${CMAKE_BINARY_DIR}/yigo_zh_CN.qm ${CMAKE_BINARY_DIR}/debroot/usr/share/yigo/translations/yigo_zh_CN.qm
        # bundled engine: only the build for the target architecture
        COMMAND ${CMAKE_COMMAND} -E copy ${CMAKE_SOURCE_DIR}/engines/KataGo/${YIGO_ENGINE_ARCH}/katago ${CMAKE_BINARY_DIR}/debroot/usr/share/yigo/engines/${YIGO_ENGINE_ARCH}/katago
        COMMAND ${CMAKE_COMMAND} -E copy ${CMAKE_SOURCE_DIR}/engines/KataGo/models/b10c384h6nbttflrs.bin.gz ${CMAKE_BINARY_DIR}/debroot/usr/share/yigo/engines/models/b10c384h6nbttflrs.bin.gz
        COMMAND ${CMAKE_COMMAND} -E copy ${CMAKE_SOURCE_DIR}/engines/KataGo/gtp.cfg ${CMAKE_BINARY_DIR}/debroot/usr/share/yigo/engines/gtp.cfg
        COMMAND ${CMAKE_COMMAND} -E copy ${CMAKE_BINARY_DIR}/packaging/debian/control ${CMAKE_BINARY_DIR}/debroot/DEBIAN/control
        COMMAND ${DPKG_DEB_EXECUTABLE} --build --root-owner-group
                ${CMAKE_BINARY_DIR}/debroot ${CMAKE_BINARY_DIR}/${YIGO_DEB_NAME}
        DEPENDS yigo translations
        COMMENT "Building ${YIGO_DEB_NAME}"
        VERBATIM
    )
endif()
