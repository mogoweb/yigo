#pragma once
#include <QChar>
#include <QPointF>

// Board <-> pixel geometry; pure QtCore, unit-testable without a GUI.
struct BoardGeometry {
    BoardGeometry(int boardSize, qreal cellPx);
    qreal cellPx() const { return m_cellPx; }
    qreal boardPx() const;                       // size cells + 1-cell margin each side
    QPointF gridToPoint(int x, int y) const;
    int pointToGrid(QPointF p) const;            // -1 if outside snap radius
    int pointToGridY(QPointF p) const;
    static int displayXToGrid(QChar letter);     // display letters skip 'I'
    static QChar gridToDisplayX(int x);

private:
    int m_size;
    qreal m_cellPx;
};
