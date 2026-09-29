#include <QtTest>
#include <QSignalSpy>
#include <QFile>
#include <QTemporaryDir>
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
    void startSendsMoveTimeBudget() {
        QTemporaryDir dir;
        // CPU builds need an explicit per-move budget, otherwise a 19x19
        // genmove is bounded only by maxVisits (~115s) and looks like a hang
        const QString log = dir.path() + QStringLiteral("/cmds.log");
        qputenv("YIGO_FAKE_LOG", log.toUtf8());
        EngineProcess ep;
        EngineConfig cfg = fakeCfg();
        QCOMPARE(cfg.moveSeconds, 5);
        QVERIFY(ep.start(cfg));
        auto readLog = [&log]() {
            QFile f(log);
            return f.open(QIODevice::ReadOnly) ? QString::fromUtf8(f.readAll())
                                               : QString();
        };
        QTRY_VERIFY(readLog().contains(QStringLiteral("time_settings 0 5 1")));
        ep.stop();

        // 0 = leave the limits to the engine: no time_settings must be sent
        const QString log2 = dir.path() + QStringLiteral("/cmds2.log");
        qputenv("YIGO_FAKE_LOG", log2.toUtf8());
        EngineProcess ep2;
        EngineConfig cfg2 = fakeCfg();
        cfg2.moveSeconds = 0;
        QVERIFY(ep2.start(cfg2));
        QTest::qWait(500);
        QFile f2(log2);
        const QString sent = f2.open(QIODevice::ReadOnly)
                                 ? QString::fromUtf8(f2.readAll()) : QString();
        QVERIFY(sent.contains(QStringLiteral("name")));
        QVERIFY(!sent.contains(QStringLiteral("time_settings")));
        ep2.stop();
        qunsetenv("YIGO_FAKE_LOG");
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
