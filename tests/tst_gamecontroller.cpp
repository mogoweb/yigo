#include <QtTest>
#include "GameController.h"
#include "Game.h"

class TestGameController : public QObject {
    Q_OBJECT
private slots:
    void handicapPoints() {
        // 19 路标准让子摆位（对角开始，5 子加天元，6-8 子加边星）
        QCOMPARE(Game::handicapPoint(19, 0), QPoint(15, 3));   // 右下
        QCOMPARE(Game::handicapPoint(19, 1), QPoint(3, 3));    // 左上
        QCOMPARE(Game::handicapPoint(19, 2), QPoint(3, 15));   // 左下
        QCOMPARE(Game::handicapPoint(19, 3), QPoint(15, 15));  // 右上
        QCOMPARE(Game::handicapPoint(19, 4), QPoint(9, 9));    // 天元
        QCOMPARE(Game::handicapPoint(9, 0), QPoint(6, 2));
        QCOMPARE(Game::handicapPoint(9, 2), QPoint(2, 6));     // 9路对角第2点
        QCOMPARE(Game::handicapPoint(9, 4), QPoint(4, 4));
        QCOMPARE(Game::handicapPoint(13, 4), QPoint(6, 6));
        QCOMPARE(Game::handicapPoint(19, 8), QPoint(9, 15));   // 第9点（底边）
        QCOMPARE(Game::handicapPoint(19, 9), QPoint(-1, -1));  // 只有9个星位
        QCOMPARE(Game::handicapPoint(5, 0), QPoint(-1, -1));   // 非 9/13/19
    }
    void setupHandicapApplies() {
        Game g(9);
        g.setupHandicap(2);
        QCOMPARE(g.board().stoneAt(6, 2), Stone::Black);
        QCOMPARE(g.board().stoneAt(2, 2), Stone::Black);
        QCOMPARE(g.rules().handicap, 2);
        QCOMPARE(g.setupStones().size(), 2);
        // 无让子清空
        Game g2(19);
        g2.setupHandicap(0);
        QCOMPARE(g2.setupStones().size(), 0);
    }
    void handicapWhiteFirst() {
        // review focus 3: 让子后白先
        Game g(9);
        g.setupHandicap(2);
        QCOMPARE(g.nextToPlay(), Stone::White);
    }
};
QTEST_GUILESS_MAIN(TestGameController)
#include "tst_gamecontroller.moc"
