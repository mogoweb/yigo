#include <QtTest>
#include "WinrateCurve.h"
#include "GameTree.h"

class TestWinrateCurve : public QObject {
    Q_OBJECT
private slots:
    void emptyCurve() {
        WinrateCurve c;
        QCOMPARE(c.count(), 0);
    }
    void rebuildFromMainLine() {
        // 构造主线 3 手，只填 analysis
        GameTree t;
        auto* n1 = t.addChild(t.root(), Stone::Black, QPoint(3, 3));
        auto* n2 = t.addChild(n1, Stone::White, QPoint(15, 15));
        auto* n3 = t.addChild(n2, Stone::Black, QPoint(2, 2));
        t.root()->analysis = AnalysisData{}; t.root()->analysis.valid = true; t.root()->analysis.winrate = 0.5;
        n1->analysis.valid = true; n1->analysis.winrate = 0.55;
        n2->analysis.valid = true; n2->analysis.winrate = 0.45;
        n3->analysis.valid = true; n3->analysis.winrate = 0.5;
        QVector<MoveNode*> line{t.root(), n1, n2, n3};
        WinrateCurve c;
        c.rebuild(line);
        QCOMPARE(c.count(), 4);
        QCOMPARE(c.points()[0].winrate, 0.5);
        QCOMPARE(c.points()[0].moveNumber, 0);
        QCOMPARE(c.points()[3].moveNumber, 3);
        QCOMPARE(c.points()[1].sideToMove, Stone::Black);
        QCOMPARE(c.points()[2].sideToMove, Stone::White);
    }
    void blunderPerSide() {
        // review focus 3: 黑跌 20% = 黑失着；白把局面从 0.3 拉回 0.5（白亏 20%）= 白失着
        GameTree t;
        auto* n1 = t.addChild(t.root(), Stone::Black, QPoint(3, 3));   // 黑走
        auto* n2 = t.addChild(n1, Stone::White, QPoint(15, 15));       // 白走
        t.root()->analysis.valid = true; t.root()->analysis.winrate = 0.5;
        n1->analysis.valid = true; n1->analysis.winrate = 0.3;   // 黑跌 20% → 黑失着
        n2->analysis.valid = true; n2->analysis.winrate = 0.5;   // 白把 0.3→0.5，白亏 20% → 白失着
        QVector<MoveNode*> line{t.root(), n1, n2};
        WinrateCurve c;
        c.rebuild(line);
        QVERIFY(c.points()[1].blunder);   // 黑失着
        QVERIFY(c.points()[2].blunder);   // 白失着
        // 阈值边界：差正好等于阈值不算失着
        n1->analysis.winrate = 0.45;      // 黑跌 5% = 阈值 → 不算
        c.rebuild(line);
        QVERIFY(!c.points()[1].blunder);
        // 未分析点（valid=false）不参与
        n2->analysis.valid = false;
        c.rebuild(line);
        QVERIFY(!c.points()[2].blunder);
    }
    void setPointOverwrites() {
        WinrateCurve c;
        c.setPoint(2, 0.7);
        QCOMPARE(c.count(), 1);
        c.setPoint(2, 0.8);   // 覆盖
        QCOMPARE(c.count(), 1);
        QCOMPARE(c.points()[0].winrate, 0.8);
        c.setPoint(1, 0.4);   // 乱序插入：保持按 moveNumber 排序
        QCOMPARE(c.count(), 2);
        QCOMPARE(c.points()[0].moveNumber, 1);
    }
};

QTEST_GUILESS_MAIN(TestWinrateCurve)
#include "tst_winratecurve.moc"
