#pragma once
#include <QVector>

#include "Board.h"
#include "GameTree.h"

struct WinratePoint {
    int moveNumber = 0;
    double winrate = 0.5;      // black's perspective 0..1
    bool blunder = false;
    Stone sideToMove = Stone::Empty;
};

// Winrate series along the game's main line with per-side blunder detection.
// Winrate is ALWAYS black's perspective (0..1) per the spec invariant.
class WinrateCurve {
public:
    void rebuild(const QVector<MoveNode*>& mainLine);
    void setPoint(int moveNumber, double winrate);
    int count() const { return m_points.size(); }
    const QVector<WinratePoint>& points() const { return m_points; }
    void setBlunderThreshold(double t) { m_threshold = t; }
    double blunderThreshold() const { return m_threshold; }

private:
    QVector<WinratePoint> m_points;
    double m_threshold = 0.05;
};
