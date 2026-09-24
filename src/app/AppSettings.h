#pragma once
#include <QByteArray>
#include <QString>

#include "EngineConfig.h"
#include "GameController.h"

// Thin QSettings wrapper for persisted app state (engine config, last game
// setup, window geometry). QtCore-only, unit-testable with an INI path.
class AppSettings {
public:
    explicit AppSettings(const QString& iniPath = QString());

    EngineConfig engineConfig() const;
    void setEngineConfig(const EngineConfig& cfg);
    GameSetup gameSetup() const;
    void setGameSetup(const GameSetup& s);
    QByteArray windowGeometry() const;
    void setWindowGeometry(const QByteArray& geo);
    // UI language: empty = follow system locale; otherwise e.g. "zh_CN", "en"
    QString language() const;
    void setLanguage(const QString& lang);

private:
    QString m_iniPath;
};
