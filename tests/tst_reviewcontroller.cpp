#include <QtTest>
#include <QSignalSpy>
#include "ReviewController.h"
#include "EngineProcess.h"
#include "SgfParser.h"

class TestReviewController : public QObject {
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
    void batchAnalyzesMainLine() {
        QString err;
        Game* g = SgfParser::parse("(;GM[1]FF[4]SZ[9];B[cc];W[gg];B[dd])", &err);
        QVERIFY2(g != nullptr, qPrintable(err));
        EngineProcess ep;
        QVERIFY(ep.start(fakeCfg()));
        ReviewController rc;
        QSignalSpy prog(&rc, &ReviewController::progressed);
        rc.startReview(g, &ep);
        QTRY_COMPARE(prog.count(), 4);   // root + 3 moves
        QCOMPARE(rc.cursor(), 4);        // cursor indexes mainLine (root=0)
        QVERIFY(rc.curve().count() >= 4);
        QTRY_VERIFY(!rc.isRunning());
        ep.stop();
        delete g;
    }
    void pauseStopsBatch() {
        QString err;
        Game* g = SgfParser::parse("(;GM[1]FF[4]SZ[9];B[cc];W[gg];B[dd])", &err);
        QVERIFY(g != nullptr);
        EngineProcess ep;
        QVERIFY(ep.start(fakeCfg()));
        ReviewController rc;
        rc.startReview(g, &ep);
        QTest::qWait(50);            // batch may already be partway through
        rc.pause();
        QTest::qWait(200);           // let in-flight frames drain
        const int c0 = rc.cursor();
        QTest::qWait(300);
        QCOMPARE(rc.cursor(), c0);   // fully paused: no further progress
        rc.resume();
        QTRY_COMPARE(rc.cursor(), 4);
        ep.stop();
        delete g;
    }
    void jumpDuringAnalysis() {
        // review focus 1: 分析进行中跳转 → 目标节点先分析，队列恢复
        QString err;
        Game* g = SgfParser::parse(
            "(;GM[1]FF[4]SZ[9];B[cc];W[gg];B[dd];B[pp];W[qq])", &err);   // 5 手
        QVERIFY(g != nullptr);
        EngineProcess ep;
        QVERIFY(ep.start(fakeCfg()));
        ReviewController rc;
        QSignalSpy prog(&rc, &ReviewController::progressed);
        rc.startReview(g, &ep);
        QTest::qWait(50);
        rc.jumpTo(1);                // 跳到第 1 手
        QTRY_COMPARE(rc.cursor(), 6);   // root + 5 手恢复后走完全部
        QCOMPARE(prog.count(), 6);      // 每手恰好一次（含 root 与被跳的第 1 手）
        ep.stop();
        delete g;
    }
    void crashStopsBatch() {
        // review focus 2: 引擎崩溃 → 停止、保留已完成
        QString err;
        Game* g = SgfParser::parse("(;GM[1]FF[4]SZ[9];B[cc];W[gg];B[dd])", &err);
        QVERIFY(g != nullptr);
        EngineProcess ep;
        QVERIFY(ep.start(fakeCfg()));
        ReviewController rc;
        rc.startReview(g, &ep);
        QTest::qWait(50);
        ep.query(999, "please crash");
        QTRY_VERIFY(!rc.isRunning());
        QVERIFY(rc.cursor() <= 4);
        ep.stop();
        delete g;
    }
    void mainLineOnly() {
        // review focus 5: 分支不进曲线
        QString err;
        Game* g = SgfParser::parse(
            "(;GM[1]FF[4]SZ[9];B[cc](;W[gg];B[dd])(;W[dd];B[gg]))", &err);
        QVERIFY(g != nullptr);
        EngineProcess ep;
        QVERIFY(ep.start(fakeCfg()));
        ReviewController rc;
        QSignalSpy prog(&rc, &ReviewController::progressed);
        rc.startReview(g, &ep);
        QTRY_COMPARE(prog.count(), 4);   // root + 主线 3 手 (B[cc];W[gg];B[dd])
        // 变着分支 (W[dd];B[gg]) 绝不进曲线：曲线 moveNumber 覆盖 0..3 且
        // 逐点等于主线手数（4 点）
        QCOMPARE(rc.curve().count(), 4);
        for (int i = 0; i < rc.curve().points().size(); ++i)
            QCOMPARE(rc.curve().points()[i].moveNumber, i);
        ep.stop();
        delete g;
    }
};

QTEST_GUILESS_MAIN(TestReviewController)
#include "tst_reviewcontroller.moc"
