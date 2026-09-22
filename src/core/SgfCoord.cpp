#include "SgfCoord.h"

namespace SgfCoord {

QPoint fromSgf(char col, char row) {
    if (col < 'a' || col > 'z' || row < 'a' || row > 'z')
        return QPoint(-1, -1);
    return QPoint(col - 'a', row - 'a');
}

QPoint fromSgf(char col, char row, int boardSize) {
    // 'tt' means pass only on boards of 19 or smaller ('t' is a valid column
    // letter on 25-board); empty value handled by the parser
    if (boardSize <= 19 && col == 't' && row == 't')
        return QPoint(-1, -1);
    return fromSgf(col, row);
}

void toSgf(QPoint pos, int boardSize, char& col, char& row) {
    if (pos.x() < 0 || pos.y() < 0) {
        col = 't';
        row = 't';
        return;
    }
    col = static_cast<char>('a' + pos.x());
    row = static_cast<char>('a' + pos.y());
}

bool isPass(char col, char row) {
    return col == 't' && row == 't';
}

} // namespace SgfCoord
