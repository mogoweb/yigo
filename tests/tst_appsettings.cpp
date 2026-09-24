#include <QtTest>
#include <QTemporaryDir>
#include "AppSettings.h"

class TestAppSettings : public QObject {
    Q_OBJECT
private slots:
    void defaultsWhenEmpty() {
        // review focus 1: 空配置 → 默认值
        QTemporaryDir dir;
        AppSettings s(dir.path() + "/settings.ini");
        const EngineConfig e = s.engineConfig();
        QCOMPARE(e.executable, QString());
        QCOMPARE(e.type, EngineConfig::KataGo);
        const GameSetup g = s.gameSetup();
        QCOMPARE(g.boardSize, 19);
        QCOMPARE(g.komi, 7.5);
        QCOMPARE(g.handicap, 0);
        QCOMPARE(g.black.kind, PlayerConfig::Human);
        QCOMPARE(g.white.kind, PlayerConfig::Human);
        QVERIFY(s.windowGeometry().isEmpty());
    }
    void engineRoundTrip() {
        QTemporaryDir dir;
        const QString path = dir.path() + "/settings.ini";
        {
            AppSettings s(path);
            EngineConfig cfg;
            cfg.type = EngineConfig::LeelaZero;
            cfg.executable = "/opt/lz/leelaz";
            cfg.baseArgs = QStringList() << "--weights" << "w.gz";
            cfg.gtpCommand = "lz-analyze";
            s.setEngineConfig(cfg);
        }
        AppSettings s2(path);   // reopen after sync
        const EngineConfig e = s2.engineConfig();
        QCOMPARE(e.type, EngineConfig::LeelaZero);
        QCOMPARE(e.executable, QString("/opt/lz/leelaz"));
        QCOMPARE(e.baseArgs, QStringList() << "--weights" << "w.gz");
        QCOMPARE(e.gtpCommand, QString("lz-analyze"));
    }
    void gameSetupRoundTrip() {
        QTemporaryDir dir;
        const QString path = dir.path() + "/settings.ini";
        {
            AppSettings s(path);
            GameSetup g;
            g.boardSize = 9; g.komi = 5.5; g.handicap = 2;
            g.black.kind = PlayerConfig::AI;
            s.setGameSetup(g);
        }
        AppSettings s2(path);
        const GameSetup g = s2.gameSetup();
        QCOMPARE(g.boardSize, 9);
        QCOMPARE(g.komi, 5.5);
        QCOMPARE(g.handicap, 2);
        QCOMPARE(g.black.kind, PlayerConfig::AI);
        QCOMPARE(g.white.kind, PlayerConfig::Human);
    }
    void invalidValuesClamped() {
        // review focus 2: 非法值 → 回退默认
        QTemporaryDir dir;
        const QString path = dir.path() + "/settings.ini";
        {   // 写入垃圾
            QSettings raw(path, QSettings::IniFormat);
            raw.setValue("game/size", 0);
            raw.setValue("game/komi", "abc");
            raw.setValue("game/handicap", 99);
        }
        AppSettings s(path);
        const GameSetup g = s.gameSetup();
        QCOMPARE(g.boardSize, 19);   // 0 非法 → 默认
        QCOMPARE(g.komi, 7.5);       // 解析失败 → 默认
        QCOMPARE(g.handicap, 0);     // 99 超界 → 0
    }
    void languageRoundTrip() {
        QTemporaryDir dir;
        const QString path = dir.path() + "/settings.ini";
        {
            AppSettings s(path);
            QCOMPARE(s.language(), QString());   // empty = follow system
            s.setLanguage("zh_CN");
        }
        AppSettings s2(path);
        QCOMPARE(s2.language(), QString("zh_CN"));
        AppSettings s3(dir.path() + "/s2.ini");
        s3.setLanguage("en");
        QCOMPARE(s3.language(), QString("en"));
        s3.setLanguage(QString());   // reset to follow-system
        QVERIFY(s3.language().isEmpty());
    }
    void geometryRoundTrip() {
        // review focus 5: 几何字节往返
        QTemporaryDir dir;
        const QString path = dir.path() + "/settings.ini";
        QByteArray geo(128, 'X');
        {
            AppSettings s(path);
            s.setWindowGeometry(geo);
        }
        AppSettings s2(path);
        QCOMPARE(s2.windowGeometry(), geo);
    }
};

QTEST_GUILESS_MAIN(TestAppSettings)
#include "tst_appsettings.moc"
