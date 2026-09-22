add_library(yigo_core STATIC
    Board.cpp
    Rules.cpp
    GameTree.cpp
    Game.cpp
)
target_include_directories(yigo_core PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})
target_link_libraries(yigo_core PUBLIC Qt5::Core)
