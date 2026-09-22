#include <QtTest>
#include "Board.h"

class TestBoard : public QObject {
    Q_OBJECT
private slots:
    void emptyBoard() {
        Board b(19);
        QCOMPARE(b.size(), 19);
        QCOMPARE(b.stoneAt(3, 3), Stone::Empty);
        QVERIFY(b.inBounds(0, 0));
        QVERIFY(b.inBounds(18, 18));
        QVERIFY(!b.inBounds(19, 0));
        QVERIFY(!b.inBounds(-1, 5));
    }
    void sizesSupported() {
        for (int s : {9, 13, 19}) {
            Board b(s);
            QCOMPARE(b.size(), s);
        }
        Board big(Board::MaxSize);
        QCOMPARE(big.size(), 25);
    }
    void opponent() {
        QCOMPARE(Stone(Stone::Black), Stone::Black);
        QCOMPARE(Board::opponent(Stone::Black), Stone::White);
        QCOMPARE(Board::opponent(Stone::White), Stone::Black);
        QCOMPARE(Board::opponent(Stone::Empty), Stone::Empty);
    }
};

QTEST_MAIN(TestBoard)
#include "tst_board.moc"
