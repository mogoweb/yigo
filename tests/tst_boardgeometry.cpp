#include <QtTest>
#include <QPointF>
#include "BoardGeometry.h"

class TestBoardGeometry : public QObject {
    Q_OBJECT
private slots:
    void gridToPoint() {
        BoardGeometry g(19, 30.0);
        // (0,0) is the top-left intersection, 1-cell margin
        QPointF p = g.gridToPoint(0, 0);
        QCOMPARE(p.x(), 30.0);
        QCOMPARE(p.y(), 30.0);
        QPointF p18 = g.gridToPoint(18, 18);
        QCOMPARE(p18.x(), 30.0 + 18 * 30.0);
        QCOMPARE(p18.y(), 30.0 + 18 * 30.0);
    }
    void pointToGridSnaps() {
        BoardGeometry g(19, 30.0);
        // snap: nearest intersection within half a cell
        QCOMPARE(g.pointToGrid(QPointF(30.0, 30.0)), 0);          // exact
        QCOMPARE(g.pointToGrid(QPointF(30.0 + 14.0, 30.0)), 0);   // 14px off -> snap
        // 16px right of intersection 0 is 14px left of intersection 1:
        // nearest-intersection snap returns 1, not -1 (mid-cell points always
        // snap to the closer line under this rule)
        QCOMPARE(g.pointToGrid(QPointF(30.0 + 16.0, 30.0)), 1);
        QCOMPARE(g.pointToGrid(QPointF(30.0 + 18 * 30.0, 30.0)), 18);
    }
    void pointToGridBounds() {
        BoardGeometry g(9, 30.0);
        QCOMPARE(g.pointToGrid(QPointF(-5.0, 30.0)), -1);           // outside
        QCOMPARE(g.pointToGrid(QPointF(30.0 + 10 * 30.0, 30.0)), -1);
        QCOMPARE(g.pointToGridY(QPointF(30.0, -5.0)), -1);
    }
    void displayLetters() {
        QCOMPARE(BoardGeometry::displayXToGrid('A'), 0);
        QCOMPARE(BoardGeometry::displayXToGrid('H'), 7);
        QCOMPARE(BoardGeometry::displayXToGrid('J'), 8);   // skips I
        QCOMPARE(BoardGeometry::displayXToGrid('T'), 18);
        QCOMPARE(BoardGeometry::displayXToGrid('a'), 0);   // lowercase ok
        QCOMPARE(BoardGeometry::displayXToGrid('I'), -1);  // no I
        QCOMPARE(BoardGeometry::displayXToGrid('U'), -1);
        QCOMPARE(BoardGeometry::gridToDisplayX(0), QChar('A'));
        QCOMPARE(BoardGeometry::gridToDisplayX(8), QChar('J'));
        QCOMPARE(BoardGeometry::gridToDisplayX(18), QChar('T'));
        QVERIFY(BoardGeometry::gridToDisplayX(-1).isNull());
        QVERIFY(BoardGeometry::gridToDisplayX(25).isNull());
    }
    void boardPx() {
        BoardGeometry g(19, 30.0);
        // 19 cells + 1-cell margin on each side: margin 30 + grid 18*30 + margin 30
        QCOMPARE(g.boardPx(), 20 * 30.0);
    }
};

QTEST_GUILESS_MAIN(TestBoardGeometry)
#include "tst_boardgeometry.moc"
