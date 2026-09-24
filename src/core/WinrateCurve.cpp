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
    // find insertion position and the previous analyzed point
    int pos = 0;
    double prev = -1.0;
    for (int i = 0; i < m_points.size(); ++i) {
        if (m_points[i].moveNumber == moveNumber) {
            m_points[i].winrate = winrate;
            recomputeBlunder(m_points[i], prev);
            return;
        }
        if (m_points[i].moveNumber < moveNumber) {
            pos = i + 1;
            prev = m_points[i].winrate;   // last point before the insertion
        }
    }
    WinratePoint p;
    p.moveNumber = moveNumber;
    p.winrate = winrate;
    recomputeBlunder(p, prev);
    m_points.insert(pos, p);
}

void WinrateCurve::recomputeBlunder(WinratePoint& p, double prev) {
    // review Important #3: blunder flags must be live on first-pass review
    if (prev < 0.0 || p.sideToMove == Stone::Empty) {
        p.blunder = false;
        return;
    }
    const double gain = p.sideToMove == Stone::Black
                            ? (p.winrate - prev)
                            : (prev - p.winrate);
    p.blunder = gain < -m_threshold;
}
