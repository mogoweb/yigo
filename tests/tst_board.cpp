#include <QtTest>
#include "Board.h"

class TestBoard : public QObject {
    Q_OBJECT
private:
    // helper: play a sequence on the board, all should succeed
    static void play(Board& b, const QVector<QPair<int,int>>& pts, Stone c) {
        for (auto p : pts) QVERIFY2(b.placeStone(p.first, p.second, c),
                                    qPrintable(QString("fail %1,%2").arg(p.first).arg(p.second)));
    }

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
    void simplePlacementAndCapture() {
        Board b(9);
        // corner: white (0,0) has only 2 liberties; black (1,0)(0,1) capture it
        QVERIFY(b.placeStone(0, 0, Stone::White));
        QVERIFY(b.placeStone(1, 0, Stone::Black));
        QVector<QPoint> cap;
        QVERIFY(b.placeStone(0, 1, Stone::Black, &cap));
        QCOMPARE(cap.size(), 1);
        QCOMPARE(b.stoneAt(0, 0), Stone::Empty);
    }
    void groupCapture() {
        Board b(9);
        // white (0,0)(1,0) surrounded by black
        play(b, {{0,0},{1,0}}, Stone::White);
        play(b, {{2,0},{0,1}}, Stone::Black);
        QVector<QPoint> cap;
        QVERIFY(b.placeStone(1, 1, Stone::Black, &cap));
        QCOMPARE(cap.size(), 2);
        QCOMPARE(b.stoneAt(0, 0), Stone::Empty);
        QCOMPARE(b.stoneAt(1, 0), Stone::Empty);
    }
    void suicideRejected() {
        Board b(9);
        play(b, {{1,0},{0,1},{1,1}}, Stone::Black);
        QVERIFY(!b.placeStone(0, 0, Stone::White));  // white (0,0) suicide
        QCOMPARE(b.stoneAt(0, 0), Stone::Empty);
        QVERIFY(b.isSuicide(0, 0, Stone::White));
    }
    void koRule() {
        // classic ko (9x9): white (2,2) has single liberty (3,2); the other
        // three neighbours of the ko point (3,2) are white, so after black
        // takes the ko his stone sits in atari
        Board k(9);
        play(k, {{1,2},{2,1},{2,3}}, Stone::Black);
        QVERIFY(k.placeStone(2, 2, Stone::White));
        play(k, {{4,2},{3,1},{3,3}}, Stone::White);
        QVERIFY(k.placeStone(3, 2, Stone::Black));           // black takes ko: captures white (2,2)
        QCOMPARE(k.stoneAt(2, 2), Stone::Empty);
        QVector<QPoint> cap;
        QVERIFY2(!k.placeStone(2, 2, Stone::White, &cap), "immediate ko recapture must be rejected");
        QCOMPARE(k.stoneAt(2, 2), Stone::Empty);             // rejected, board unchanged
        QCOMPARE(k.stoneAt(3, 2), Stone::Black);
        // after an intervening exchange elsewhere, recapture becomes legal
        QVERIFY(k.placeStone(0, 0, Stone::White));
        QVERIFY(k.placeStone(8, 8, Stone::Black));
        QVERIFY(k.placeStone(2, 2, Stone::White, &cap));
        QCOMPARE(cap.size(), 1);
        QCOMPARE(k.stoneAt(3, 2), Stone::Empty);
    }
    void superkoRejection() {
        // triple-ko cycle (period 6): three kos around the board; each side
        // captures at a different ko each turn, and after 6 takes the position
        // repeats exactly. No take is an immediate recapture, so simple ko
        // allows the whole cycle; superko (full history) must reject the
        // 7th take that closes the loop.
        Board s(9);
        s.setSuperko(true);
        // koA: W surrounds B(2,1) [atari, lib (2,2)]; B surrounds (2,2) so the
        //      capturing W stone is itself in atari
        s.setupStone(2, 0, Stone::White); s.setupStone(1, 1, Stone::White); s.setupStone(3, 1, Stone::White);
        s.setupStone(2, 1, Stone::Black);
        s.setupStone(2, 3, Stone::Black); s.setupStone(1, 2, Stone::Black); s.setupStone(3, 2, Stone::Black);
        // koB: same shape one row block lower
        s.setupStone(2, 5, Stone::White); s.setupStone(1, 6, Stone::White); s.setupStone(3, 6, Stone::White);
        s.setupStone(2, 6, Stone::Black);
        s.setupStone(2, 8, Stone::Black); s.setupStone(1, 7, Stone::Black); s.setupStone(3, 7, Stone::Black);
        // koC (colors swapped): B surrounds W(6,3) [atari, lib (6,4)];
        //     W surrounds (6,4) so the capturing B stone is itself in atari
        s.setupStone(6, 2, Stone::Black); s.setupStone(5, 3, Stone::Black); s.setupStone(7, 3, Stone::Black);
        s.setupStone(6, 3, Stone::White);
        s.setupStone(6, 5, Stone::White); s.setupStone(5, 4, Stone::White); s.setupStone(7, 4, Stone::White);

        QVERIFY(s.placeStone(2, 2, Stone::White));           // 1: W takes koA, caps B(2,1)
        QCOMPARE(s.stoneAt(2, 1), Stone::Empty);
        QVERIFY(s.placeStone(6, 4, Stone::Black));           // 2: B takes koC, caps W(6,3)
        QCOMPARE(s.stoneAt(6, 3), Stone::Empty);
        QVERIFY(s.placeStone(2, 7, Stone::White));           // 3: W takes koB, caps B(2,6)
        QCOMPARE(s.stoneAt(2, 6), Stone::Empty);
        QVERIFY(s.placeStone(2, 1, Stone::Black));           // 4: B retakes koA, caps W(2,2)
        QCOMPARE(s.stoneAt(2, 2), Stone::Empty);
        QVERIFY(s.placeStone(6, 3, Stone::White));           // 5: W retakes koC, caps B(6,4)
        QCOMPARE(s.stoneAt(6, 4), Stone::Empty);
        QVERIFY(s.placeStone(2, 6, Stone::Black));           // 6: B retakes koB, caps W(2,7)
        QCOMPARE(s.stoneAt(2, 7), Stone::Empty);
        // move 7 would recreate the position after move 1
        QVERIFY2(!s.isLegal(2, 2, Stone::White), "superko must reject position repetition");
        QCOMPARE(s.stoneAt(2, 2), Stone::Empty);
        QVERIFY(s.positionHash() != 0);
        // sanity: without superko the whole cycle is legal, incl. move 7
        Board t(9);
        t.setupStone(2, 0, Stone::White); t.setupStone(1, 1, Stone::White); t.setupStone(3, 1, Stone::White);
        t.setupStone(2, 1, Stone::Black);
        t.setupStone(2, 3, Stone::Black); t.setupStone(1, 2, Stone::Black); t.setupStone(3, 2, Stone::Black);
        t.setupStone(2, 5, Stone::White); t.setupStone(1, 6, Stone::White); t.setupStone(3, 6, Stone::White);
        t.setupStone(2, 6, Stone::Black);
        t.setupStone(2, 8, Stone::Black); t.setupStone(1, 7, Stone::Black); t.setupStone(3, 7, Stone::Black);
        t.setupStone(6, 2, Stone::Black); t.setupStone(5, 3, Stone::Black); t.setupStone(7, 3, Stone::Black);
        t.setupStone(6, 3, Stone::White);
        t.setupStone(6, 5, Stone::White); t.setupStone(5, 4, Stone::White); t.setupStone(7, 4, Stone::White);
        QVERIFY(t.placeStone(2, 2, Stone::White));
        QVERIFY(t.placeStone(6, 4, Stone::Black));
        QVERIFY(t.placeStone(2, 7, Stone::White));
        QVERIFY(t.placeStone(2, 1, Stone::Black));
        QVERIFY(t.placeStone(6, 3, Stone::White));
        QVERIFY(t.placeStone(2, 6, Stone::Black));
        QVERIFY2(t.placeStone(2, 2, Stone::White), "simple ko allows the triple-ko cycle");
    }
    void occupiedRejected() {
        Board b(9);
        QVERIFY(b.placeStone(4, 4, Stone::Black));
        QVERIFY(!b.isLegal(4, 4, Stone::White));
        QVERIFY(!b.placeStone(4, 4, Stone::White));
        QCOMPARE(b.stoneAt(4, 4), Stone::Black);
    }
    void clearRestores() {
        Board b(9);
        play(b, {{0,0},{8,8}}, Stone::Black);
        b.clear();
        QCOMPARE(b.stoneAt(0, 0), Stone::Empty);
        QVERIFY(b.isLegal(0, 0, Stone::White));
    }
};

QTEST_MAIN(TestBoard)
#include "tst_board.moc"
