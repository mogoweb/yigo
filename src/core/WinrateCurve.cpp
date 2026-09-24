#include "WinrateCurve.h"
#include <algorithm>

void WinrateCurve::rebuild(const QVector<MoveNode*>& mainLine) {
    m_points.clear();
    for (MoveNode* n : mainLine) {
        WinratePoint p;
        p.moveNumber = n->moveNumber;
        p.winrate = n->analysis.valid ? n->analysis.winrate : 0.5;
        p.sideToMove = n->color;
        m_points.append(p);
    }
    // blunder detection compares against the previous ANALYZED point
    // (root counts as analyzed when it carries a valid analysis)
    int lastAnalyzed = (!mainLine.isEmpty() && mainLine[0] && mainLine[0]->analysis.valid)
                           ? 0 : -1;
    for (int i = 1; i < m_points.size(); ++i) {
        MoveNode* n = mainLine[i];
        if (!n || !n->analysis.valid) continue;
        if (lastAnalyzed >= 0) {
            const double prev = m_points[lastAnalyzed].winrate;
            const double cur = m_points[i].winrate;
            const Stone side = m_points[i].sideToMove;
            // black's gain = cur - prev; white's gain = prev - cur
            const double gain = (side == Stone::Black) ? (cur - prev) : (prev - cur);
            m_points[i].blunder = gain < -m_threshold;
        }
        lastAnalyzed = i;
    }
}

void WinrateCurve::setPoint(int moveNumber, double winrate) {
    for (WinratePoint& p : m_points)
        if (p.moveNumber == moveNumber) {
            p.winrate = winrate;
            return;
        }
    WinratePoint p;
    p.moveNumber = moveNumber;
    p.winrate = winrate;
    m_points.append(p);
    std::sort(m_points.begin(), m_points.end(),
              [](const WinratePoint& a, const WinratePoint& b) {
                  return a.moveNumber < b.moveNumber;
              });
}
