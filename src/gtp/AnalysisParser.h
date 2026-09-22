#pragma once
#include <QString>
#include <QPoint>

#include "EngineConfig.h"

// Parses engine analysis frames into unified AnalysisData (black's-perspective
// winrate 0..1). Key-value based, order-independent.
class AnalysisParser {
public:
    static AnalysisData parseInfo(const QString& line, EngineConfig::EngineType type,
                                  int boardSize = 19);
    static QPoint parseMove(const QString& body, int boardSize);
};
