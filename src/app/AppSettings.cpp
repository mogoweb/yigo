#include "AppSettings.h"
#include <QFileInfo>
#include <QSettings>

AppSettings::AppSettings(const QString& iniPath) : m_iniPath(iniPath) {}

static QSettings makeSettings(const QString& iniPath) {
    return iniPath.isEmpty()
               ? QSettings(QSettings::IniFormat, QSettings::UserScope, "YiGo", "YiGo")
               : QSettings(iniPath, QSettings::IniFormat);
}

// KataGo build + weights bundled in engines/KataGo (see README), so a fresh
// checkout can play and analyze without hunting for an engine. Empty when the
// bundle is absent (e.g. a packaged install).
static QString bundledEngineDir() {
#ifdef YIGO_ENGINE_DIR
    const QString dir = QStringLiteral(YIGO_ENGINE_DIR);
    if (QFileInfo::exists(dir + QStringLiteral("/katago"))) return dir;
#endif
    return QString();
}

EngineConfig AppSettings::engineConfig() const {
    QSettings s = makeSettings(m_iniPath);
    EngineConfig cfg;
    cfg.type = s.value("engine/type", int(EngineConfig::KataGo)).toInt() == 1
                   ? EngineConfig::LeelaZero
                   : EngineConfig::KataGo;
    const QString bundled = bundledEngineDir();
    // KataGo needs -config or it aborts before the GTP handshake
    const QStringList defaultArgs =
        bundled.isEmpty()
            ? QStringList()
            : QStringList() << QStringLiteral("gtp")
                            << QStringLiteral("-model")
                            << bundled + QStringLiteral("/models/b10c384h6nbttflrs.bin.gz")
                            << QStringLiteral("-config")
                            << bundled + QStringLiteral("/gtp.cfg");
    cfg.executable = s.value("engine/executable",
                             bundled.isEmpty() ? QString()
                                               : bundled + QStringLiteral("/katago"))
                         .toString();
    cfg.baseArgs = s.value("engine/args", defaultArgs).toStringList();
    cfg.gtpCommand = cfg.type == EngineConfig::KataGo
                         ? QStringLiteral("kata-analyze interval 50")
                         : QStringLiteral("lz-analyze");
    return cfg;
}

void AppSettings::setEngineConfig(const EngineConfig& cfg) {
    QSettings s = makeSettings(m_iniPath);
    s.setValue("engine/type", int(cfg.type));
    s.setValue("engine/executable", cfg.executable);
    s.setValue("engine/args", cfg.baseArgs);
    s.sync();
}

GameSetup AppSettings::gameSetup() const {
    QSettings s = makeSettings(m_iniPath);
    GameSetup g;
    const int size = s.value("game/size", 19).toInt();
    g.boardSize = (size == 9 || size == 13 || size == 19) ? size : 19;
    bool komiOk = false;
    const double komi = s.value("game/komi", 7.5).toDouble(&komiOk);
    g.komi = (komiOk && komi >= 0.0 && komi <= 30.0) ? komi : 7.5;
    const int hc = s.value("game/handicap", 0).toInt();
    g.handicap = (hc >= 0 && hc <= 9) ? hc : 0;
    g.black.kind = s.value("game/blackAI", false).toBool() ? PlayerConfig::AI
                                                           : PlayerConfig::Human;
    g.white.kind = s.value("game/whiteAI", false).toBool() ? PlayerConfig::AI
                                                           : PlayerConfig::Human;
    return g;
}

void AppSettings::setGameSetup(const GameSetup& g) {
    QSettings s = makeSettings(m_iniPath);
    s.setValue("game/size", g.boardSize);
    s.setValue("game/komi", g.komi);
    s.setValue("game/handicap", g.handicap);
    s.setValue("game/blackAI", g.black.kind == PlayerConfig::AI);
    s.setValue("game/whiteAI", g.white.kind == PlayerConfig::AI);
    s.sync();
}

QByteArray AppSettings::windowGeometry() const {
    QSettings s = makeSettings(m_iniPath);
    return s.value("window/geometry").toByteArray();
}

void AppSettings::setWindowGeometry(const QByteArray& geo) {
    QSettings s = makeSettings(m_iniPath);
    s.setValue("window/geometry", geo);
    s.sync();
}

QString AppSettings::language() const {
    QSettings s = makeSettings(m_iniPath);
    return s.value("ui/language").toString();
}

void AppSettings::setLanguage(const QString& lang) {
    QSettings s = makeSettings(m_iniPath);
    s.setValue("ui/language", lang);
    s.sync();
}
