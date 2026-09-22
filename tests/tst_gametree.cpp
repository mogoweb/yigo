#include <QtTest>
#include "GameTree.h"

class TestGameTree : public QObject {
    Q_OBJECT
private slots:
    void rootOnly() {
        GameTree t;
        QVERIFY(t.root() != nullptr);
        QCOMPARE(t.root()->moveNumber, 0);
        QCOMPARE(t.nodeCount(), 1);
        QCOMPARE(t.mainLine().size(), 0);
    }
    void addChildAndMainLine() {
        GameTree t;
        auto* n1 = t.addChild(t.root(), Stone::Black, QPoint(3, 3));
        auto* n2 = t.addChild(n1, Stone::White, QPoint(15, 15));
        QCOMPARE(t.nodeCount(), 3);
        QCOMPARE(n1->moveNumber, 1);
        QCOMPARE(n2->moveNumber, 2);
        QCOMPARE(n2->parent, n1);
        auto ml = t.mainLine();
        QCOMPARE(ml.size(), 2);
        QCOMPARE(ml[0], n1);
        QCOMPARE(ml[1], n2);
    }
    void branchAndRemove() {
        GameTree t;
        auto* n1 = t.addChild(t.root(), Stone::Black, QPoint(3, 3));
        auto* main2 = t.addChild(n1, Stone::White, QPoint(15, 15));
        auto* var2 = t.addChild(n1, Stone::White, QPoint(2, 2));   // variation
        QCOMPARE(n1->children.size(), 2);
        QCOMPARE(n1->children[0], main2);   // first added is main line
        // remove variation
        QVERIFY(t.removeChild(var2));
        QCOMPARE(n1->children.size(), 1);
        QCOMPARE(t.nodeCount(), 3);
        // remove main line -> children[0] empty, mainLine shrinks
        QVERIFY(t.removeChild(main2));
        QCOMPARE(t.nodeCount(), 2);
        QCOMPARE(t.mainLine().size(), 1);
        QCOMPARE(t.mainLine()[0], n1);
    }
    void removeSubtree() {
        GameTree t;
        auto* n1 = t.addChild(t.root(), Stone::Black, QPoint(3, 3));
        t.addChild(n1, Stone::White, QPoint(15, 15));
        t.addChild(n1, Stone::White, QPoint(2, 2));
        QCOMPARE(t.nodeCount(), 4);
        QVERIFY(t.removeChild(n1));   // whole subtree deleted
        QCOMPARE(t.nodeCount(), 1);
    }
    void pathTo() {
        GameTree t;
        auto* n1 = t.addChild(t.root(), Stone::Black, QPoint(3, 3));
        auto* n2 = t.addChild(n1, Stone::White, QPoint(15, 15));
        auto p = t.pathTo(n2);
        QCOMPARE(p.size(), 2);
        QCOMPARE(p[0], n1);
        QCOMPARE(p[1], n2);
    }
    void pathToRoot() {
        GameTree t;
        auto* n1 = t.addChild(t.root(), Stone::Black, QPoint(3, 3));
        auto p = t.pathTo(t.root());
        QCOMPARE(p.size(), 0);
        auto p2 = t.pathTo(n1);
        QCOMPARE(p2.size(), 1);
    }
    void removeNullAndRoot() {
        GameTree t;
        QVERIFY(!t.removeChild(nullptr));
        QVERIFY(!t.removeChild(t.root()));   // root cannot be removed
    }
};

QTEST_MAIN(TestGameTree)
#include "tst_gametree.moc"
