#pragma once
#include <QString>
#include <QStringList>
#include <QVector>
#include <QPoint>

#include "Board.h"
#include "GameTree.h"

struct EngineConfig {
    enum EngineType { KataGo, LeelaZero } type = KataGo;
    QString executable;
    QStringList baseArgs;        // e.g. {"-model","x.bin.gz","-config","y.cfg"}
    QString gtpCommand;          // KataGo: "kata-analyze interval 50"; LZ: "lz-analyze"
};

struct AnalysisQuery {
    Stone color = Stone::Black;
    int maxVisits = 0;           // 0 = engine default
    QVector<QPoint> avoidMoves;  // future extension
};
