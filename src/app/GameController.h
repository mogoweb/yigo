#pragma once
#include <QString>

#include "Game.h"
#include "EngineProcess.h"

struct PlayerConfig {
    enum Kind { Human, AI } kind = Human;
};

struct GameSetup {
    int boardSize = 19;
    double komi = 7.5;
    int handicap = 0;
    PlayerConfig black;
    PlayerConfig white;
};
