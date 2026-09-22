#pragma once
#include <QObject>
#include <QString>
#include <QStringList>
#include <QTimer>

#include "EngineConfig.h"
#include "AnalysisParser.h"
#include "Game.h"

class QProcess;
class GtpClient;

// Engine subprocess manager: serial command queue + analysis state machine.
// Engine crash only emits a signal — the main program must survive (spec §1).
class EngineProcess : public QObject {
    Q_OBJECT
public:
    explicit EngineProcess(QObject* parent = nullptr);
    ~EngineProcess() override;
    bool start(const EngineConfig& cfg);
    void stop();                                  // graceful quit -> kill on timeout
    bool isRunning() const;
    bool isAnalyzing() const;
    const QString& engineName() const { return m_engineName; }
    void query(quint64 id, const QString& command);       // one-shot GTP command
    // sync the engine's board with the game, then analyze the current node
    void analyzePosition(const Game& game, const AnalysisQuery& q);
    void startAnalysis(const AnalysisQuery& q);           // stream on current board
    void stopAnalysis();

signals:
    void connected(const QString& engineName, const QStringList& supportedCommands);
    void queryFinished(quint64 id, bool success, const QString& body);
    void analysisUpdate(const AnalysisData& data);
    void crashed(int exitCode);
    void errorOccurred(const QString& msg);

private slots:
    void onReadyRead();

private:
    enum class State { Idle, Analyzing, Stopping };
    void handleResponse(quint64 id, bool success, const QString& body);
    void doStartAnalysis();
    void resetSessionState();
    void armQueryTimeout();

    EngineConfig m_cfg;
    State m_state = State::Idle;
    QProcess* m_proc = nullptr;
    GtpClient* m_client = nullptr;
    quint64 m_nextId = 1;
    QString m_engineName;
    QStringList m_supported;
    AnalysisQuery m_analysisQuery;
    int m_boardSize = 19;
    QStringList m_positionQueue;     // pending position-sync commands
    QTimer m_queryTimeout;           // spec §6: 30s per-command timeout
    QByteArray m_lineBuffer;         // partial-line carry-over for analysis
    bool m_userStop = false;
};
