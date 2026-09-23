#include <QtTest>
#include <QSignalSpy>
#include "GameController.h"
#include "Game.h"

class TestGameController : public QObject {
    Q_OBJECT
private:
    EngineConfig fakeCfg() const {
        EngineConfig cfg;
        cfg.type = EngineConfig::KataGo;
        cfg.executable = "/bin/bash";
        cfg.baseArgs = QStringList() << QStringLiteral(FAKE_ENGINE) << "basic";
        cfg.gtpCommand = QStringLiteral("kata-analyze interval 50");
        return cfg;
    }

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
    void humanTurnFirstWhenHumanBlack() {
        GameSetup setup;
        setup.boardSize = 9;
        setup.black.kind = PlayerConfig::Human;
        setup.white.kind = PlayerConfig::AI;
        GameController gc;
        EngineProcess ep;
        QVERIFY(ep.start(fakeCfg()));
        gc.attachEngine(&ep);
        gc.newGame(setup);
        QCOMPARE(gc.phase(), GameController::Phase::HumanTurn);
        // 人类落子后轮到 AI，AI 回 D4（9路 = (3,5)）后回到人类
        QVERIFY(gc.humanPlay(QPoint(2, 2)));
        QTRY_COMPARE(gc.phase(), GameController::Phase::EngineThinking);
        QTRY_COMPARE(gc.phase(), GameController::Phase::HumanTurn);
        QCOMPARE(gc.game()->board().stoneAt(3, 5), Stone::White);   // AI执白，D4 = (3,5)@9路
        ep.stop();
    }
    void genmovePassEnds() {
        qputenv("YIGO_FAKE_GENMOVE", "pass");
        GameSetup setup;
        setup.boardSize = 9;
        setup.black.kind = PlayerConfig::Human;
        setup.white.kind = PlayerConfig::AI;
        GameController gc;
        EngineProcess ep;
        QVERIFY(ep.start(fakeCfg()));
        gc.attachEngine(&ep);
        QSignalSpy over(&gc, &GameController::gameOver);
        gc.newGame(setup);
        QVERIFY(gc.humanPlay(QPoint(2, 2)));
        QTRY_COMPARE(gc.phase(), GameController::Phase::HumanTurn);
        QVERIFY(gc.humanPlay(QPoint(-1, -1)));   // human pass → 两虚手终局
        QTRY_COMPARE(gc.phase(), GameController::Phase::GameOver);
        QCOMPARE(over.count(), 1);
        QCOMPARE(gc.endReason(), QString("two passes"));
        qunsetenv("YIGO_FAKE_GENMOVE");
        ep.stop();
    }
    void genmoveResignEnds() {
        qputenv("YIGO_FAKE_GENMOVE", "resign");
        GameSetup setup;
        setup.boardSize = 9;
        setup.black.kind = PlayerConfig::Human;
        setup.white.kind = PlayerConfig::AI;
        GameController gc;
        EngineProcess ep;
        QVERIFY(ep.start(fakeCfg()));
        gc.attachEngine(&ep);
        QSignalSpy over(&gc, &GameController::gameOver);
        gc.newGame(setup);
        QVERIFY(gc.humanPlay(QPoint(2, 2)));
        QTRY_COMPARE(gc.phase(), GameController::Phase::GameOver);
        QCOMPARE(gc.winner(), Stone::Black);   // AI resign → 黑胜
        QCOMPARE(gc.endReason(), QString("resign"));
        qunsetenv("YIGO_FAKE_GENMOVE");
        ep.stop();
    }
    void bothAiAlternates() {
        // review focus 1: 双 AI 不卡死；fake 恒回 D4，重试 3 次后 engine error 终局
        GameSetup setup;
        setup.boardSize = 9;
        setup.black.kind = PlayerConfig::AI;
        setup.white.kind = PlayerConfig::AI;
        GameController gc;
        EngineProcess ep;
        QVERIFY(ep.start(fakeCfg()));
        gc.attachEngine(&ep);
        gc.newGame(setup);
        QCOMPARE(gc.phase(), GameController::Phase::EngineThinking);
        QTRY_COMPARE_WITH_TIMEOUT(gc.phase(), GameController::Phase::GameOver, 30000);
        QCOMPARE(gc.endReason(), QString("engine error"));
        ep.stop();
    }
    void crashDuringThinkingRecovers() {
        // review focus 4: 引擎思考中崩溃 → 回退 HumanTurn
        GameSetup setup;
        setup.boardSize = 9;
        setup.black.kind = PlayerConfig::Human;
        setup.white.kind = PlayerConfig::AI;
        GameController gc;
        EngineProcess ep;
        QVERIFY(ep.start(fakeCfg()));
        gc.attachEngine(&ep);
        gc.newGame(setup);
        QVERIFY(gc.humanPlay(QPoint(2, 2)));
        QTRY_COMPARE(gc.phase(), GameController::Phase::EngineThinking);
        ep.query(999, "please crash");   // fake engine kills itself
        QTRY_COMPARE(gc.phase(), GameController::Phase::HumanTurn);
        QVERIFY(gc.humanPlay(QPoint(4, 4)));   // 离线继续
        ep.stop();
    }
    void humanPlayRejectedOutsideTurn() {
        GameSetup setup;
        setup.boardSize = 9;
        setup.black.kind = PlayerConfig::AI;
        setup.white.kind = PlayerConfig::Human;
        GameController gc;
        EngineProcess ep;
        QVERIFY(ep.start(fakeCfg()));
        gc.attachEngine(&ep);
        QSignalSpy rej(&gc, &GameController::moveRejected);
        gc.newGame(setup);                    // AI 执黑先走
        QCOMPARE(gc.phase(), GameController::Phase::EngineThinking);
        QVERIFY(!gc.humanPlay(QPoint(2, 2))); // 未轮到人
        QCOMPARE(rej.count(), 1);
        ep.stop();
    }
    void undoInPlayRemovesAIPair() {
        // review focus 5: 悔棋退到玩家上一手之前
        GameSetup setup;
        setup.boardSize = 9;
        setup.black.kind = PlayerConfig::Human;
        setup.white.kind = PlayerConfig::AI;
        GameController gc;
        EngineProcess ep;
        QVERIFY(ep.start(fakeCfg()));
        gc.attachEngine(&ep);
        gc.newGame(setup);
        QVERIFY(gc.humanPlay(QPoint(2, 2)));                          // 人1
        QTRY_COMPARE(gc.phase(), GameController::Phase::HumanTurn);   // AI 已回
        QCOMPARE(gc.game()->currentNode()->moveNumber, 2);
        QVERIFY(gc.undoInPlay());
        QCOMPARE(gc.game()->currentNode()->moveNumber, 0);            // 退到人1之前
        QCOMPARE(gc.phase(), GameController::Phase::HumanTurn);
        QVERIFY(gc.humanPlay(QPoint(4, 4)));   // 继续可下
        QTRY_COMPARE(gc.phase(), GameController::Phase::HumanTurn);
        ep.stop();
    }
    void engineThinkingUndoRejected() {
        GameSetup setup;
        setup.boardSize = 9;
        setup.black.kind = PlayerConfig::Human;
        setup.white.kind = PlayerConfig::AI;
        GameController gc;
        EngineProcess ep;
        QVERIFY(ep.start(fakeCfg()));
        gc.attachEngine(&ep);
        gc.newGame(setup);
        QVERIFY(gc.humanPlay(QPoint(2, 2)));
        QCOMPARE(gc.phase(), GameController::Phase::EngineThinking);
        QCOMPARE(gc.undoInPlay(), false);   // EngineThinking
        QTRY_COMPARE(gc.phase(), GameController::Phase::HumanTurn);
        ep.stop();
    }
};
QTEST_GUILESS_MAIN(TestGameController)
#include "tst_gamecontroller.moc"
