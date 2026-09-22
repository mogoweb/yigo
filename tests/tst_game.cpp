#include <QtTest>
#include <QRandomGenerator>
#include "Game.h"

class TestGame : public QObject {
    Q_OBJECT
private slots:
    void playAndUndoRedo() {
        Game g(9);
        QVERIFY(g.currentNode()->parent == nullptr);   // root
        auto* n1 = g.play(QPoint(2, 2), Stone::Black);
        QVERIFY(n1 != nullptr);
        QCOMPARE(g.currentNode(), n1);
        QCOMPARE(g.board().stoneAt(2, 2), Stone::Black);
        QCOMPARE(g.nextToPlay(), Stone::White);
        QVERIFY(g.undo());
        QCOMPARE(g.currentNode(), g.tree().root());
        QCOMPARE(g.board().stoneAt(2, 2), Stone::Empty);
        QVERIFY(g.redo());
        QCOMPARE(g.currentNode(), n1);
        QCOMPARE(g.board().stoneAt(2, 2), Stone::Black);
    }
    void illegalMoveRejected() {
        Game g(9);
        QVERIFY(g.play(QPoint(0, 0), Stone::Black) != nullptr);
        QVERIFY(g.play(QPoint(0, 0), Stone::White) == nullptr);   // occupied
        QVERIFY(g.currentNode()->moveNumber == 1);
    }
    void captureAccumulates() {
        Game g(9);
        QVERIFY(g.play(QPoint(0, 0), Stone::White) != nullptr);
        QVERIFY(g.play(QPoint(1, 0), Stone::Black) != nullptr);
        QVERIFY(g.play(QPoint(0, 1), Stone::Black) != nullptr);   // captures white (0,0)
        QCOMPARE(g.blackCaptures(), 1);
        QCOMPARE(g.whiteCaptures(), 0);
    }
    void branchAndVariant() {
        Game g(9);
        auto* n1 = g.play(QPoint(2, 2), Stone::Black);
        g.undo();
        auto* var = g.play(QPoint(3, 3), Stone::Black);   // branch at same point
        QVERIFY(var != nullptr);
        QVERIFY(var != n1);
        QCOMPARE(g.tree().root()->children.size(), 2);
        g.undo();                          // var is a leaf: back to root first
        QCOMPARE(g.currentNode(), g.tree().root());
        QVERIFY(g.redo());                 // main line -> n1
        QCOMPARE(g.currentNode(), n1);
        g.undo();                          // back to root
        QVERIFY(g.redoVariant(1));         // variation -> var
        QCOMPARE(g.currentNode(), var);
        QVERIFY(g.undo());                 // back to root
        QVERIFY(g.redo());                 // main line again
        QCOMPARE(g.currentNode(), n1);
    }
    void goToRandomConsistency() {
        // random 200 steps (fixed seed) incl. branches/undo/redo:
        // goTo any node must match full replay position point-by-point
        Game g(9);
        QRandomGenerator rng(42);
        QVector<MoveNode*> all{g.tree().root()};
        Stone color = Stone::Black;
        for (int i = 0; i < 200; ++i) {
            int x = rng.bounded(9), y = rng.bounded(9);
            auto* n = g.play(QPoint(x, y), color);
            if (n) { all.append(n); color = Board::opponent(color); }
            else if (rng.bounded(4) == 0 && g.currentNode()->parent) { g.undo(); color = Board::opponent(color); }
        }
        // full replay baseline: independently build a Game replaying pathTo
        for (auto* node : all) {
            Game ref(9);
            for (auto* step : g.tree().pathTo(node)) {
                if (step == g.tree().root()) continue;
                QVERIFY2(ref.play(step->pos, step->color) != nullptr,
                         qPrintable(QString("replay failed at node #%1").arg(step->moveNumber)));
            }
            g.goTo(node);
            for (int x = 0; x < 9; ++x)
                for (int y = 0; y < 9; ++y)
                    if (g.board().stoneAt(x, y) != ref.board().stoneAt(x, y))
                        QFAIL(qPrintable(QString("mismatch at %1,%2 node #%3").arg(x).arg(y).arg(node->moveNumber)));
        }
    }
    void snapshotCacheUsed() {
        // jump correctness is guaranteed by goToRandomConsistency; here verify
        // jump around a long game stays consistent (exercises snapshot path)
        Game g(19);
        for (int i = 0; i < 100; ++i) {
            QVERIFY(g.play(QPoint(i % 19, i / 19 % 19), (i % 2) ? Stone::White : Stone::Black) != nullptr);
        }
        g.goTo(g.tree().root());       // back to root
        g.goTo(g.currentNode());       // self-jump harmless
        QCOMPARE(g.currentNode()->moveNumber, 0);
        // jump to move 60 node and verify board matches full replay
        MoveNode* n60 = nullptr;
        {
            Game ref(19);
            auto ml = g.tree().mainLine();
            QVERIFY(ml.size() >= 60);
            n60 = ml[59];
            for (auto* step : g.tree().pathTo(n60))
                QVERIFY(ref.play(step->pos, step->color) != nullptr);
            g.goTo(n60);
            for (int x = 0; x < 19; ++x)
                for (int y = 0; y < 19; ++y)
                    QCOMPARE(g.board().stoneAt(x, y), ref.board().stoneAt(x, y));
        }
    }
    void passMove() {
        Game g(9);
        auto* n = g.play(QPoint(-1, -1), Stone::Black);
        QVERIFY(n != nullptr);
        QCOMPARE(g.currentNode()->moveNumber, 1);
        QCOMPARE(g.currentNode()->pos, QPoint(-1, -1));
        QCOMPARE(g.nextToPlay(), Stone::White);
        QCOMPARE(g.board().stoneAt(4, 4), Stone::Empty);   // board unchanged
        QVERIFY(g.play(QPoint(-1, -1), Stone::White));     // two passes in a row ok
        QCOMPARE(g.currentNode()->moveNumber, 2);
    }
    void capturesUndoRedo() {
        // capture counters must follow undo/redo consistently;
        // note: white (0,0) is captured by black (0,1) at move 3
        Game g(9);
        QVERIFY(g.play(QPoint(0, 0), Stone::White) != nullptr);
        QVERIFY(g.play(QPoint(1, 0), Stone::Black) != nullptr);
        QVERIFY(g.play(QPoint(0, 1), Stone::Black) != nullptr);   // capture here
        QCOMPARE(g.blackCaptures(), 1);
        QVERIFY(g.undo());                                        // back to move 2
        QCOMPARE(g.blackCaptures(), 0);
        QVERIFY(g.redo());                                        // move 3 again
        QCOMPARE(g.blackCaptures(), 1);
    }
};

QTEST_MAIN(TestGame)
#include "tst_game.moc"
