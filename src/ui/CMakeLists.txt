add_executable(yigo
    ../main.cpp
    MainWindow.cpp
    BoardView.cpp
    BoardGeometry.cpp
    EnginePanel.cpp
    NewGameDialog.cpp
    ChartWinrate.cpp
)
add_dependencies(yigo translations)
target_compile_definitions(yigo PRIVATE
    YIGO_TRANSLATIONS_DIR="${CMAKE_BINARY_DIR}"
    YIGO_APP_VERSION="0.5.0")
target_include_directories(yigo PRIVATE ${CMAKE_CURRENT_SOURCE_DIR})
target_link_libraries(yigo PRIVATE yigo_core Qt5::Widgets)
