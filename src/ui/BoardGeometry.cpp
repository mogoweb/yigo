#include "BoardGeometry.h"

BoardGeometry::BoardGeometry(int boardSize, qreal cellPx)
    : m_size(boardSize), m_cellPx(cellPx) {}

qreal BoardGeometry::boardPx() const {
    return (m_size + 1) * m_cellPx;
}

QPointF BoardGeometry::gridToPoint(int x, int y) const {
    return QPointF((x + 1) * m_cellPx, (y + 1) * m_cellPx);
}

int BoardGeometry::pointToGrid(QPointF p) const {
    const qreal v = p.x() / m_cellPx - 1.0;
    const int g = qRound(v);
    if (g < 0 || g >= m_size) return -1;
    if (qAbs(v - g) > 0.5) return -1;   // snap radius: half a cell
    return g;
}

int BoardGeometry::pointToGridY(QPointF p) const {
    const qreal v = p.y() / m_cellPx - 1.0;
    const int g = qRound(v);
    if (g < 0 || g >= m_size) return -1;
    if (qAbs(v - g) > 0.5) return -1;
    return g;
}

int BoardGeometry::displayXToGrid(QChar letter) {
    const char c = letter.toUpper().toLatin1();
    if (c >= 'A' && c <= 'H') return c - 'A';
    if (c >= 'J' && c <= 'T') return c - 'A' - 1;
    return -1;
}

QChar BoardGeometry::gridToDisplayX(int x) {
    if (x < 0 || x > 24) return QChar();
    return QChar(static_cast<char>('A' + x + (x >= 8 ? 1 : 0)));
}
