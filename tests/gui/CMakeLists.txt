# GUI probe tests: offscreen-render BoardView and verify paint/click alignment.
# These need QtWidgets/QtGui and launch a QApplication (offscreen platform).
find_package(Qt5 5.11 REQUIRED COMPONENTS Gui Widgets Test)

set(GUI_PROBE_LIBS yigo_core Qt5::Core Qt5::Gui Qt5::Widgets Qt5::Test)
set(GUI_PROBE_INCLUDES ${CMAKE_SOURCE_DIR}/src/ui ${CMAKE_SOURCE_DIR}/src/core)

set(CMAKE_AUTOMOC ON)

function(yigo_add_gui_probe name)
    add_executable(${name} ${name}.cpp
        ${CMAKE_SOURCE_DIR}/src/ui/BoardView.cpp
        ${CMAKE_SOURCE_DIR}/src/ui/BoardGeometry.cpp)
    target_include_directories(${name} PRIVATE ${GUI_PROBE_INCLUDES})
    target_link_libraries(${name} PRIVATE ${GUI_PROBE_LIBS})
    # offscreen platform: no display needed, works in CI and headless shells
    add_test(NAME ${name} COMMAND ${name})
    set_tests_properties(${name} PROPERTIES
        ENVIRONMENT "QT_QPA_PLATFORM=offscreen")
endfunction()

yigo_add_gui_probe(paint_probe)
yigo_add_gui_probe(click_probe)
# colscan is a diagnostic tool (prints where lines landed), not a pass/fail test
add_executable(colscan EXCLUDE_FROM_ALL colscan.cpp
    ${CMAKE_SOURCE_DIR}/src/ui/BoardView.cpp
    ${CMAKE_SOURCE_DIR}/src/ui/BoardGeometry.cpp)
target_include_directories(colscan PRIVATE ${GUI_PROBE_INCLUDES})
target_link_libraries(colscan PRIVATE ${GUI_PROBE_LIBS})
