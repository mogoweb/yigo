#include "EngineProcess.h"
#include "GtpClient.h"
#include <QProcess>
#include <QTimer>
#include <QDateTime>
#include <QRegularExpression>

EngineProcess::EngineProcess(QObject* parent) : QObject(parent) {}

EngineProcess::~EngineProcess() {
    if (m_proc) {
        m_proc->kill();
        m_proc->waitForFinished(1000);
    }
}

bool EngineProcess::start(const EngineConfig& cfg) {
    m_cfg = cfg;
    m_proc = new QProcess(this);
    m_proc->setProgram(cfg.executable);
    m_proc->setArguments(cfg.baseArgs);
    m_client = new GtpClient(m_proc, this);
    connect(m_client, &GtpClient::responseReceived, this, &EngineProcess::handleResponse);
    connect(m_proc, &QProcess::readyReadStandardOutput, this, &EngineProcess::onReadyRead);
    connect(m_proc, &QProcess::errorOccurred, this, [this](QProcess::ProcessError e) {
        if (e == QProcess::FailedToStart) {
            Q_EMIT errorOccurred(QStringLiteral("Engine failed to start: ")
                                 + m_cfg.executable);
        }
    });
    connect(m_proc, qOverload<int, QProcess::ExitStatus>(&QProcess::finished), this,
            [this](int code, QProcess::ExitStatus st) {
                m_state = State::Idle;
                m_engineName.clear();
                if (st == QProcess::CrashExit)
                    Q_EMIT crashed(code);
            });
    m_proc->start();
    if (!m_proc->waitForStarted(3000)) {
        Q_EMIT errorOccurred(QStringLiteral("Engine did not start: ") + m_cfg.executable);
        delete m_proc;
        m_proc = nullptr;
        return false;
    }
    // handshake: name then list_commands; handleResponse pairs them by shape
    query(m_nextId++, "name");
    query(m_nextId++, "list_commands");
    return true;
}

void EngineProcess::stop() {
    if (!m_proc || m_proc->state() == QProcess::NotRunning) return;
    m_state = State::Idle;
    m_proc->write("0 quit\n");
    if (!m_proc->waitForFinished(2000)) {
        m_proc->kill();
        m_proc->waitForFinished(1000);
    }
}

bool EngineProcess::isRunning() const {
    return m_proc && m_proc->state() != QProcess::NotRunning;
}

bool EngineProcess::isAnalyzing() const {
    return m_state == State::Analyzing;
}

void EngineProcess::query(quint64 id, const QString& command) {
    if (m_client) m_client->sendCommand(command, id);
}

void EngineProcess::startAnalysis(const AnalysisQuery& q) {
    m_analysisQuery = q;
    doStartAnalysis();
}

void EngineProcess::doStartAnalysis() {
    if (!m_client) return;
    m_state = State::Analyzing;
    // pause the response decoder: analysis frames are bare "info" lines that
    // would otherwise be swallowed into GtpClient's \n\n framing buffer
    m_client->setPaused(true);
    m_client->sendCommand(m_cfg.gtpCommand, m_nextId++);
}

void EngineProcess::stopAnalysis() {
    if (m_state != State::Analyzing) return;
    // per spec §4: interrupt the analyze stream by sending a lightweight
    // command; its response ends the stream (KataGo behavior)
    m_state = State::Stopping;
    m_client->setPaused(false);
    m_client->sendCommand(QStringLiteral("protocol_version"), m_nextId++);
    QTimer::singleShot(2000, this, [this] {
        if (m_state == State::Stopping) m_state = State::Idle;   // timeout fallback
    });
}

void EngineProcess::onReadyRead() {
    // analyze streams emit bare "info ..." lines outside the \n\n response
    // protocol — parse them here and emit throttled updates (~10 Hz per spec).
    // GtpClient is paused during analysis so it does not drain these bytes.
    if (m_state != State::Analyzing) return;
    const QByteArray all = m_proc->readAllStandardOutput();
    int start = 0;
    while (start < all.size()) {
        const int nl = all.indexOf('\n', start);
        if (nl < 0) break;
        const QByteArray line = all.mid(start, nl - start);
        start = nl + 1;
        if (!line.startsWith("info")) {
            // blank line = end of an analysis frame batch
            continue;
        }
        const AnalysisData d = AnalysisParser::parseInfo(
            QString::fromUtf8(line), m_cfg.type, 19);
        if (d.valid)
            Q_EMIT analysisUpdate(d);
    }
}

void EngineProcess::handleResponse(quint64 id, bool success, const QString& body) {
    Q_UNUSED(id);
    // handshake: name then list_commands arrive first (sent in start());
    // list_commands is recognized by having multiple space-separated commands
    if (m_engineName.isEmpty() && success && m_supported.isEmpty()) {
        m_engineName = body.trimmed();
        if (!m_supported.isEmpty())
            Q_EMIT connected(m_engineName, m_supported);
        return;
    }
    if (m_supported.isEmpty() && success) {
        m_supported = body.split('\n', QString::SkipEmptyParts);
        if (!m_engineName.isEmpty())
            Q_EMIT connected(m_engineName, m_supported);
        return;
    }
    if (m_state == State::Stopping) {
        // the interrupt probe command answered: stream is over
        m_state = State::Idle;
    }
    Q_EMIT queryFinished(id, success, body);
}
