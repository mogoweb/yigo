#include <QtTest>
#include <QSignalSpy>
#include "EngineProcess.h"

class TestEngineProcess : public QObject {
    Q_OBJECT
private:
    EngineConfig fakeCfg(const QString& mode = "basic") const {
        EngineConfig cfg;
        cfg.type = EngineConfig::KataGo;
        cfg.executable = "/bin/bash";
        cfg.baseArgs = QStringList() << QStringLiteral(FAKE_ENGINE) << mode;
        return cfg;
    }

private slots:
    void initTestCase() {
        qRegisterMetaType<AnalysisData>("AnalysisData");
    }
    void startAndIdentify() {
        EngineProcess ep;
        QSignalSpy connSpy(&ep, &EngineProcess::connected);
        QVERIFY(ep.start(fakeCfg()));
        QTRY_COMPARE(ep.isRunning(), true);
        QTRY_COMPARE(connSpy.count(), 1);
        QCOMPARE(connSpy.first().first().toString(), QString("FakeEngine"));
        ep.stop();
        QTRY_COMPARE(ep.isRunning(), false);
    }
    void queryRoundTrip() {
        EngineProcess ep;
        QVERIFY(ep.start(fakeCfg()));
        QSignalSpy fin(&ep, &EngineProcess::queryFinished);
        ep.query(10, "genmove");
        QTRY_VERIFY(fin.count() >= 1);
        bool found = false;
        for (const auto& call : fin) {
            if (call.at(0).toULongLong() == quint64(10)) {
                QCOMPARE(call.at(1).toBool(), true);
                QCOMPARE(call.at(2).toString(), QString("D4"));
                found = true;
            }
        }
        QVERIFY(found);
        ep.stop();
    }
    void analysisStreamEmitsUpdates() {
        EngineProcess ep;
        EngineConfig cfg = fakeCfg();
        cfg.gtpCommand = QStringLiteral("kata-analyze interval 50");
        QVERIFY(ep.start(cfg));
        QSignalSpy upd(&ep, &EngineProcess::analysisUpdate);
        AnalysisQuery q;
        q.color = Stone::Black;
        ep.startAnalysis(q);
        QTRY_VERIFY(upd.count() >= 1);   // fake engine streams one frame batch
        // QProcess may deliver line-by-line: each update carries one candidate
        const auto data = upd.first().first().value<AnalysisData>();
        QVERIFY(data.valid);
        QCOMPARE(data.winrate, 0.55);
        QTRY_VERIFY(upd.count() >= 2);   // second candidate arrives as its own frame
        QVERIFY(ep.isAnalyzing());
        ep.stopAnalysis();
        QTRY_COMPARE(ep.isAnalyzing(), false);
        ep.stop();
    }
    void crashResetsState() {
        EngineProcess ep;
        QVERIFY(ep.start(fakeCfg("basic")));
        QSignalSpy crashSpy(&ep, &EngineProcess::crashed);
        ep.query(1, "please crash");   // fake engine kills itself
        QTRY_VERIFY(crashSpy.count() >= 1);
        QCOMPARE(ep.isRunning(), false);
        QCOMPARE(ep.isAnalyzing(), false);   // state reset, no hang
    }
    void startFailsForBadPath() {
        EngineProcess ep;
        EngineConfig bad;
        bad.executable = "/nonexistent/engine";
        QSignalSpy errSpy(&ep, &EngineProcess::errorOccurred);
        QVERIFY(!ep.start(bad));
        ep.stop();
    }
};

QTEST_GUILESS_MAIN(TestEngineProcess)
#include "tst_engineprocess.moc"
