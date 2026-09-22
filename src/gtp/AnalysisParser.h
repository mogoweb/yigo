#pragma once
#include <QString>
#include <QStringList>
#include <QPoint>

#include "EngineConfig.h"
#include "Game.h"

// Parses engine analysis frames into unified AnalysisData (black's-perspective
// winrate 0..1). Key-value based, order-independent.
class AnalysisParser {
public:
    static AnalysisData parseInfo(const QString& line, EngineConfig::EngineType type,
                                  int boardSize = 19);
    static QPoint parseMove(const QString& body, int boardSize);
    // inverse of parseMove: grid coords -> GTP text ("D4"); pass -> "pass"
    static QString posToGtp(QPoint pos, int boardSize);
    // full GTP command list to sync an engine with the game's current position:
    // boardsize, clear_board, komi, play for every move on the path to current
    static QStringList positionCommands(const Game& game);
};
