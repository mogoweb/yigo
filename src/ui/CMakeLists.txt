add_executable(yigo
    ../main.cpp
    MainWindow.cpp
    BoardView.cpp
    BoardGeometry.cpp
    EnginePanel.cpp
)
target_include_directories(yigo PRIVATE ${CMAKE_CURRENT_SOURCE_DIR})
target_link_libraries(yigo PRIVATE yigo_core Qt5::Widgets)
