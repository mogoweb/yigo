#pragma once
#include <QChar>
#include <QPointF>

// Board <-> pixel geometry; pure QtCore, unit-testable without a GUI.
// One struct carries both cell size and board origin so painting and click
// mapping can never drift apart (the origin is the top-left of the board
// area inside the widget).
struct BoardGeometry {
    BoardGeometry(int boardSize, qreal cellPx,
                  const QPointF& origin = QPointF(0, 0));
    // fit a board into a view: cell = min(w,h)/(size+1), centered
    static BoardGeometry forView(int boardSize, qreal viewW, qreal viewH);

    qreal cellPx() const { return m_cellPx; }
    QPointF origin() const { return m_origin; }
    qreal boardPx() const;                       // size cells + 1-cell margin each side
    QPointF gridToPoint(int x, int y) const;
    int pointToGrid(QPointF p) const;            // -1 if outside snap radius
    int pointToGridY(QPointF p) const;
    static int displayXToGrid(QChar letter);     // display letters skip 'I'
    static QChar gridToDisplayX(int x);

private:
    int m_size;
    qreal m_cellPx;
    QPointF m_origin;
};
