#include "Rules.h"
#include <QQueue>
#include <QSet>

QPair<int, int> Rules::territory(const Board& board) const {
    const int size = board.size();
    QSet<QPoint> visited;
    int blackTerritory = 0;
    int whiteTerritory = 0;
    static const int dx[] = {1, -1, 0, 0};
    static const int dy[] = {0, 0, 1, -1};
    for (int y = 0; y < size; ++y) {
        for (int x = 0; x < size; ++x) {
            if (board.stoneAt(x, y) != Stone::Empty) continue;
            const QPoint start(x, y);
            if (visited.contains(start)) continue;
            // BFS the connected empty region
            QQueue<QPoint> queue;
            queue.enqueue(start);
            visited.insert(start);
            QVector<QPoint> region;
            bool touchesBlack = false;
            bool touchesWhite = false;
            while (!queue.isEmpty()) {
                const QPoint p = queue.dequeue();
                region.append(p);
                for (int i = 0; i < 4; ++i) {
                    const int nx = p.x() + dx[i];
                    const int ny = p.y() + dy[i];
                    if (!board.inBounds(nx, ny)) continue;
                    const Stone s = board.stoneAt(nx, ny);
                    if (s == Stone::Black) touchesBlack = true;
                    else if (s == Stone::White) touchesWhite = true;
                    else {
                        const QPoint np(nx, ny);
                        if (!visited.contains(np)) {
                            visited.insert(np);
                            queue.enqueue(np);
                        }
                    }
                }
            }
            if (touchesBlack && !touchesWhite) blackTerritory += region.size();
            else if (touchesWhite && !touchesBlack) whiteTerritory += region.size();
        }
    }
    return qMakePair(blackTerritory, whiteTerritory);
}

ScoreResult Rules::score(const Board& board) const {
    return score(board, 0, 0);
}

ScoreResult Rules::score(const Board& board, int blackCaptures, int whiteCaptures) const {
    ScoreResult result;
    if (m_cfg.ruleSet == RulesConfig::Chinese) {
        // chinese: area scoring = stones on board + surrounded territory
        int blackStones = 0;
        int whiteStones = 0;
        const int size = board.size();
        for (int y = 0; y < size; ++y) {
            for (int x = 0; x < size; ++x) {
                const Stone s = board.stoneAt(x, y);
                if (s == Stone::Black) ++blackStones;
                else if (s == Stone::White) ++whiteStones;
            }
        }
        auto terr = territory(board);
        result.blackScore = blackStones + terr.first;
        result.whiteScore = whiteStones + terr.second;
    } else {
        // japanese: territory scoring = surrounded territory + prisoners
        auto terr = territory(board);
        result.blackScore = terr.first + whiteCaptures;
        result.whiteScore = terr.second + blackCaptures;
    }
    result.blackMargin = result.blackScore - result.whiteScore - m_cfg.komi;
    return result;
}
