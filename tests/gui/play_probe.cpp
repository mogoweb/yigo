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
#include "EnginePanel.h"
#include "MainWindow.h"

#include <QComboBox>
#include <QDialog>
#include <QDockWidget>
#include <QLabel>
#include <QPointer>

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

    // engine state must stay visible in the main window (panel moved into
    // the dialog): a status-bar label carries the connected engine name,
    // and no dock may host the EnginePanel anymore
    bool noDock = true;
    for (QDockWidget* d : mw.findChildren<QDockWidget*>())
        if (d->widget() == mw.findChild<EnginePanel*>()) noDock = false;
    std::printf("engine panel not docked: %s\n", noDock ? "OK" : "FAIL");
    failures += noDock ? 0 : 1;

    bool statusLabel = false;
    for (QLabel* l : mw.findChildren<QLabel*>()) {
        if (l->window() == &mw
            && l->text().contains(QStringLiteral("FakeEngine"))) {
            statusLabel = true;
            break;
        }
    }
    std::printf("engine name in main-window status bar: %s\n",
                statusLabel ? "OK" : "FAIL");
    failures += statusLabel ? 0 : 1;

    // strength tier UI: switching to Strong must carry 15s in config(),
    // push a live time_settings into the running engine, and persist
    EnginePanel* panel = mw.findChild<EnginePanel*>();
    QComboBox* strength = panel
        ? panel->findChild<QComboBox*>(QStringLiteral("strengthCombo")) : nullptr;
    bool tierCfg = false;
    if (strength) {
        strength->setCurrentIndex(EngineConfig::TierStrong);
        tierCfg = panel->config().moveSeconds == 15;
    }
    std::printf("tier switch updates config: %s\n", tierCfg ? "OK" : "FAIL");
    failures += tierCfg ? 0 : 1;

    bool live = false;
    for (int i = 0; i < 50 && !live; ++i) {
        live = readLog(logPath).contains(QStringLiteral("time_settings 0 15 1"));
        if (!live) QTest::qWait(100);
    }
    std::printf("tier switch reaches running engine: %s\n", live ? "OK" : "FAIL");
    failures += live ? 0 : 1;

    AppSettings persisted(dir.path() + "/settings.ini");
    const bool saved = persisted.engineConfig().moveSeconds == 15;
    std::printf("tier change persists: %s\n", saved ? "OK" : "FAIL");
    failures += saved ? 0 : 1;


    // engine settings dialog: single non-modal instance hosting the panel
    EnginePanel* panelRef = mw.findChild<EnginePanel*>();
    QPointer<EnginePanel> guard = panelRef;
    QMetaObject::invokeMethod(&mw, "onEngineSettings");
    QTest::qWait(50);
    const QList<QDialog*> dialogs = mw.findChildren<QDialog*>();
    const bool opened = dialogs.size() == 1 && dialogs.first()->isVisible()
                        && dialogs.first()->findChild<EnginePanel*>() == panelRef;
    std::printf("settings dialog hosts the panel: %s\n", opened ? "OK" : "FAIL");
    failures += opened ? 0 : 1;

    QDialog* addr = dialogs.isEmpty() ? nullptr : dialogs.first();
    QMetaObject::invokeMethod(&mw, "onEngineSettings");   // second open
    QTest::qWait(50);
    const bool single = mw.findChildren<QDialog*>().size() == 1
                        && mw.findChild<QDialog*>() == addr;
    std::printf("second open reuses the dialog: %s\n", single ? "OK" : "FAIL");
    failures += single ? 0 : 1;

    if (addr) addr->close();
    QTest::qWait(50);
    const bool survives = !guard.isNull() && (!addr || !addr->isVisible());
    std::printf("close hides but keeps the panel: %s\n", survives ? "OK" : "FAIL");
    failures += survives ? 0 : 1;

    QMetaObject::invokeMethod(&mw, "onEngineSettings");   // reopen
    QTest::qWait(50);
    const bool reopen = mw.findChild<QDialog*>() == addr && addr && addr->isVisible()
                        && !guard.isNull();
    std::printf("reopen shows the same dialog: %s\n", reopen ? "OK" : "FAIL");
    failures += reopen ? 0 : 1;
    std::printf(failures == 0 ? "play_probe: all OK\n" : "play_probe: %d FAIL\n", failures);
    return failures == 0 ? 0 : 1;
}
