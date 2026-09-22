#include <QtTest>
#include "Board.h"
#include "Rules.h"

class TestRules : public QObject {
    Q_OBJECT
private slots:
    void emptyBoardChinese() {
        Board b(9);
        Rules r(RulesConfig{});   // Chinese 7.5
        auto s = r.score(b);
        // empty board: no stones, no territory: 0 - 0 - 7.5 = black loses 7.5
        QCOMPARE(s.blackScore, 0.0);
        QCOMPARE(s.whiteScore, 0.0);
        QCOMPARE(s.blackMargin, -7.5);
    }
    void territoryFloodFill() {
        Board b(9);
        // black fully encloses the 3x3 corner region (0..2, 0..2) with a wall:
        // (3,0)(3,1)(3,2)(3,3)(2,3)(1,3)(0,3); the wall's right/outside face
        // touches the rest of the board which touches nothing else -> that
        // outer region also borders only black, so black = 9 + 63
        QVector<QPair<int,int>> black = {{3,0},{3,1},{3,2},{3,3},{2,3},{1,3},{0,3}};
        for (auto p : black) QVERIFY(b.placeStone(p.first, p.second, Stone::Black));
        Rules r(RulesConfig{});
        auto t = r.territory(b);
        QCOMPARE(t.first, 74);   // enclosed 9 + open region 65 (7 stones on board)
        QCOMPARE(t.second, 0);
        // white stone inside the open region splits ownership: the open region
        // now touches both colors -> neutral; black keeps only the enclosed 9
        QVERIFY(b.placeStone(8, 8, Stone::White));
        auto t2 = r.territory(b);
        QCOMPARE(t2.first, 9);
        QCOMPARE(t2.second, 0);
    }
    void neutralPoint() {
        Board b(9);
        // empty point touching both colors -> neutral, no ownership
        // black (0,1)(0,3), white (1,2): (0,2) neighbors (0,1)B (0,3)B (1,2)W -> mixed
        QVERIFY(b.placeStone(0, 1, Stone::Black));
        QVERIFY(b.placeStone(0, 3, Stone::Black));
        QVERIFY(b.placeStone(1, 2, Stone::White));
        Rules r(RulesConfig{});
        auto t = r.territory(b);
        QCOMPARE(t.first, 0);
        QCOMPARE(t.second, 0);
    }
    void chineseScoring() {
        Board b(9);
        // black: 5 stones, white: 3 stones, no territory enclosed
        QVector<QPair<int,int>> black = {{2,2},{3,2},{2,3},{3,3},{7,7}};
        QVector<QPair<int,int>> white = {{5,5},{6,5},{5,6}};
        for (auto p : black) QVERIFY(b.placeStone(p.first, p.second, Stone::Black));
        for (auto p : white) QVERIFY(b.placeStone(p.first, p.second, Stone::White));
        Rules r(RulesConfig{});
        auto s = r.score(b);
        // chinese: black = 5 stones + 0 territory; white = 3 + 0
        QCOMPARE(s.blackScore, 5.0);
        QCOMPARE(s.whiteScore, 3.0);
        QCOMPARE(s.blackMargin, 5.0 - 3.0 - 7.5);
    }
    void japaneseScoring() {
        Board b(9);
        QVector<QPair<int,int>> black = {{2,2},{3,2},{2,3},{3,3},{7,7}};
        QVector<QPair<int,int>> white = {{5,5},{6,5},{5,6}};
        for (auto p : black) QVERIFY(b.placeStone(p.first, p.second, Stone::Black));
        for (auto p : white) QVERIFY(b.placeStone(p.first, p.second, Stone::White));
        RulesConfig cfg;
        cfg.ruleSet = RulesConfig::Japanese;
        cfg.komi = 6.5;
        Rules r(cfg);
        auto s = r.score(b);
        // japanese: territory only, no captures: black 0, white 0 -> margin -6.5
        QCOMPARE(s.blackScore, 0.0);
        QCOMPARE(s.whiteScore, 0.0);
        QCOMPARE(s.blackMargin, -6.5);
    }
private:
    static QPair<int,int> r_territory(const Board& b) {
        Rules r(RulesConfig{});
        return r.territory(b);
    }
};

QTEST_MAIN(TestRules)
#include "tst_rules.moc"
