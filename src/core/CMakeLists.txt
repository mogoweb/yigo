add_library(yigo_core STATIC
    Board.cpp
    Rules.cpp
    GameTree.cpp
    Game.cpp
    SgfCoord.cpp
    SgfParser.cpp
    WinrateCurve.cpp
    ${CMAKE_SOURCE_DIR}/src/app/MainWindowLogic.cpp
    ${CMAKE_SOURCE_DIR}/src/app/GameController.cpp
    ${CMAKE_SOURCE_DIR}/src/app/ReviewController.cpp
    ${CMAKE_SOURCE_DIR}/src/gtp/GtpClient.cpp
    ${CMAKE_SOURCE_DIR}/src/gtp/AnalysisParser.cpp
    ${CMAKE_SOURCE_DIR}/src/gtp/EngineProcess.cpp
)
target_include_directories(yigo_core PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}
    ${CMAKE_SOURCE_DIR}/src/app
    ${CMAKE_SOURCE_DIR}/src/gtp
)
target_link_libraries(yigo_core PUBLIC Qt5::Core)
