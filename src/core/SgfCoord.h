#pragma once
#include <QPoint>

namespace SgfCoord {
    // 'a'..'z' -> 0..25; two-parameter version does NOT treat 'tt' as pass
    QPoint fromSgf(char col, char row);
    // pass ('tt' for boards <= 19) -> (-1,-1) when boardSize check applies
    QPoint fromSgf(char col, char row, int boardSize);
    // encode board coords to SGF chars; pass (-1,-1) -> "tt"
    void toSgf(QPoint pos, int boardSize, char& col, char& row);
    bool isPass(char col, char row);
}
