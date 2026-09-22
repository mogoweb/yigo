#pragma once
#include <QObject>
#include <QString>
#include <QStringList>

#include "EngineConfig.h"
#include "AnalysisParser.h"

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
    void startAnalysis(const AnalysisQuery& q);           // kata-analyze stream
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

    EngineConfig m_cfg;
    State m_state = State::Idle;
    QProcess* m_proc = nullptr;
    GtpClient* m_client = nullptr;
    quint64 m_nextId = 1;
    QString m_engineName;
    QStringList m_supported;
    AnalysisQuery m_analysisQuery;
};
