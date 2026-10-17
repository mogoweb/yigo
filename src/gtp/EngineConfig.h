#pragma once
#include <QString>
#include <QStringList>
#include <QVector>
#include <QPoint>

#include "Board.h"
#include "GameTree.h"

struct EngineConfig {
    enum EngineType { KataGo, LeelaZero } type = KataGo;
    // UI strength tiers: the selector is cosmetic, engine/moveSeconds stays
    // the persisted truth (round-tripped in AppSettings)
    enum StrengthTier { TierFast = 0, TierStandard = 1, TierStrong = 2 };
    static int secondsForTier(int tier) {
        switch (tier) {
        case TierFast: return 2;
        case TierStrong: return 15;
        default: return 5;   // Standard + any invalid value
        }
    }
    static int tierForSeconds(int secs) {
        if (secs == secondsForTier(TierFast)) return TierFast;
        if (secs == secondsForTier(TierStrong)) return TierStrong;
        return TierStandard;   // exact match required; 10 etc. shows Standard
    }
    QString executable;
    QStringList baseArgs;        // e.g. {"-model","x.bin.gz","-config","y.cfg"}
    QString gtpCommand;          // KataGo: "kata-analyze interval 50"; LZ: "lz-analyze"
    // per-move search budget sent as GTP "time_settings 0 <n> 1"; a CPU engine
    // without it falls back to maxVisits and a 19x19 move takes minutes.
    // 0 = leave the limits to the engine.
    int moveSeconds = 5;
};

struct AnalysisQuery {
    Stone color = Stone::Black;
    int maxVisits = 0;           // 0 = engine default
    QVector<QPoint> avoidMoves;  // future extension
};
