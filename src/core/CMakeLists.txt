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
    ${CMAKE_SOURCE_DIR}/src/app/AppSettings.cpp
    ${CMAKE_SOURCE_DIR}/src/gtp/GtpClient.cpp
    ${CMAKE_SOURCE_DIR}/src/gtp/AnalysisParser.cpp
    ${CMAKE_SOURCE_DIR}/src/gtp/EngineProcess.cpp
)
target_include_directories(yigo_core PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}
    ${CMAKE_SOURCE_DIR}/src/app
    ${CMAKE_SOURCE_DIR}/src/gtp
)
# bundled KataGo: gtp.cfg / models/ at the root, the engine build itself under
# <arch>/ (see CMakeLists.txt). AppSettings also looks for the same layout in
# the packaged install dir, where the .deb already carries one architecture.
target_compile_definitions(yigo_core PUBLIC
    YIGO_ENGINE_DIR="${CMAKE_SOURCE_DIR}/engines/KataGo"
    YIGO_ENGINE_ARCH="${YIGO_ENGINE_ARCH}"
    YIGO_INSTALLED_ENGINE_DIR="/usr/share/yigo/engines")
target_link_libraries(yigo_core PUBLIC Qt5::Core)
