// Play-mode wiring probe: verifies the human-vs-engine chain end to end.
//   1. Quick Start (19/13/9) auto-starts the engine when it is not running
//   2. an engine game must NOT stream kata-analyze (genmove owns the engine;
//      regression guard for "start button only analyzed", 2026-10-17)
//   3. after the human moves, the engine answers through genmove
// The fake engine records every received command into YIGO_FAKE_LOG, which
// doubles as a black-box assertion surface for both directions.
// Usage: play_probe [exit code 0 = all OK]
#include <QApplication>
#include <QFile>
#include <QTemporaryDir>
#include <QtTest>
#include <cstdio>
#include "AppSettings.h"
#include "Board.h"
#include "MainWindow.h"

static QString readLog(const QString& path) {
    QFile f(path);
    return f.open(QIODevice::ReadOnly) ? QString::fromUtf8(f.readAll()) : QString();
}

int main(int argc, char** argv) {
    qputenv("QT_QPA_PLATFORM", "offscreen");
    QApplication app(argc, argv);
    QTemporaryDir dir;
    const QString logPath = dir.path() + "/engine.log";
    qputenv("YIGO_FAKE_LOG", logPath.toUtf8().constData());

    // hermetic settings: fake engine, default setup (human B vs engine W)
    AppSettings settings(dir.path() + "/settings.ini");
    EngineConfig cfg;
    cfg.executable = "/bin/bash";
    cfg.baseArgs = QStringList() << QStringLiteral(FAKE_ENGINE) << "basic";
    cfg.gtpCommand = QStringLiteral("kata-analyze interval 50");
    settings.setEngineConfig(cfg);

    MainWindow mw(&settings);   // settings injection seam for probes
    mw.show();
    QTest::qWait(50);

    int failures = 0;
    // Quick Start 9x9: engine must be started automatically -> handshake
    QMetaObject::invokeMethod(&mw, "onNewGame9");
    bool started = false;
    for (int i = 0; i < 50 && !started; ++i) {
        started = readLog(logPath).contains("name");
        if (!started) QTest::qWait(100);
    }
    std::printf("quick start auto-runs engine: %s\n", started ? "OK" : "FAIL");
    failures += started ? 0 : 1;

    // human passes -> engine replies white D4 (9路 = (3,5))
    QMetaObject::invokeMethod(&mw, "onPass");
    bool replied = false;
    for (int i = 0; i < 50 && !replied; ++i) {
        replied = mw.game() && mw.game()->currentNode()->moveNumber >= 2
                  && mw.game()->board().stoneAt(3, 5) == Stone::White;
        if (!replied) QTest::qWait(100);
    }
    std::printf("engine genmove reply lands: %s\n", replied ? "OK" : "FAIL");
    failures += replied ? 0 : 1;

    // engine game: no kata-analyze may reach the engine
    const bool noAnalyze = !readLog(logPath).contains("kata-analyze");
    std::printf("no analysis stream during play: %s\n", noAnalyze ? "OK" : "FAIL");
    failures += noAnalyze ? 0 : 1;

    std::printf(failures == 0 ? "play_probe: all OK\n" : "play_probe: %d FAIL\n", failures);
    return failures == 0 ? 0 : 1;
}
