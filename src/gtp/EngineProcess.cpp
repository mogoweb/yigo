#include "EngineProcess.h"
#include "GtpClient.h"
#include <QProcess>
#include <QDateTime>

EngineProcess::EngineProcess(QObject* parent)
    : QObject(parent) {
    // spec §6: GTP response timeout — a request with no answer in 30s is
    // voided and the state reset so the UI can retry
    m_queryTimeout.setSingleShot(true);
    m_queryTimeout.setInterval(30000);
    connect(&m_queryTimeout, &QTimer::timeout, this, [this] {
        if (m_state == State::Analyzing || m_state == State::Stopping)
            m_state = State::Idle;
        // spec §6: 复位后允许重试 — 必须把读取权交还 GtpClient，否则解码器停在
        // paused、EngineProcess 又不是 Analyzing，之后所有命令都无人收取
        if (m_client) m_client->setPaused(false);
        m_positionQueue.clear();
        Q_EMIT errorOccurred(QStringLiteral("GTP response timeout (30s)"));
    });
}

EngineProcess::~EngineProcess() {
    if (m_proc) {
        m_userStop = true;
        m_proc->kill();
        m_proc->waitForFinished(1000);
    }
}

void EngineProcess::resetSessionState() {
    m_state = State::Idle;
    m_engineName.clear();
    m_supported.clear();
    m_positionQueue.clear();
    m_lineBuffer.clear();
    m_queryTimeout.stop();
}

bool EngineProcess::start(const EngineConfig& cfg) {
    // re-entry safe: tear down any previous session first (review fix I3)
    if (m_proc) {
        m_userStop = true;
        m_proc->kill();
        m_proc->waitForFinished(1000);
        delete m_client;
        m_client = nullptr;
        delete m_proc;
        m_proc = nullptr;
    }
    resetSessionState();

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
                if (st == QProcess::CrashExit && !m_userStop)
                    Q_EMIT crashed(code);
            });
    m_proc->start();
    if (!m_proc->waitForStarted(3000)) {
        // review fix I4: no dangling client over a dead process
        delete m_client;
        m_client = nullptr;
        delete m_proc;
        m_proc = nullptr;
        Q_EMIT errorOccurred(QStringLiteral("Engine did not start: ") + m_cfg.executable);
        return false;
    }
    // handshake: name then list_commands; handleResponse pairs them by shape
    armQueryTimeout();
    query(m_nextId++, "name");
    query(m_nextId++, "list_commands");
    return true;
}

void EngineProcess::stop() {
    if (!m_proc || m_proc->state() == QProcess::NotRunning) return;
    m_userStop = true;   // do not report our own kill as a crash (review fix M10)
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

void EngineProcess::sendCommandSequence(const QStringList& cmds) {
    for (const QString& c : cmds)
        query(m_nextId++, c);
    // review I7: the sequence includes a genmove — arm the 30s timeout so a
    // wedged engine cannot leave the caller waiting forever
    if (!cmds.isEmpty())
        armQueryTimeout();
}

void EngineProcess::analyzePosition(const Game& game, const AnalysisQuery& q) {
    if (!isRunning()) {
        Q_EMIT errorOccurred(QStringLiteral("Engine not running"));
        return;
    }
    m_analysisQuery = q;
    m_boardSize = game.boardSize();
    // queue position sync commands; analysis starts when the queue drains
    m_positionQueue = AnalysisParser::positionCommands(game);
    if (m_state != State::Analyzing) {
        m_state = State::Stopping;   // reuse: route responses to queue draining
        m_client->setPaused(false);
        armQueryTimeout();
        query(m_nextId++, m_positionQueue.takeFirst());
    } else {
        // interrupt current analysis, then the Stopping->Idle transition will
        // drain the queue
        m_state = State::Stopping;
        m_client->setPaused(false);
        m_client->sendCommand(QStringLiteral("protocol_version"), m_nextId++);
    }
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
    m_lineBuffer.clear();
    QString cmd = m_cfg.gtpCommand;
    // the optional player must come right after the command name; appended at
    // the end it is a parse error — KataGo answers
    // "? Could not parse analyze arguments: 'interval 50 B'"
    const QString color = m_analysisQuery.color == Stone::Black
                              ? QStringLiteral(" B") : QStringLiteral(" W");
    const int sp = cmd.indexOf(' ');
    cmd = (sp < 0) ? cmd + color : cmd.left(sp) + color + cmd.mid(sp);
    m_client->sendCommand(cmd, m_nextId++);
    // no timeout here: an interval analysis stream never produces a framed
    // response by design, so arming one only fires a spurious error and resets
    // the state machine out from under the live stream
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

void EngineProcess::armQueryTimeout() {
    m_queryTimeout.start();
}

void EngineProcess::setQueryTimeout(int ms) {
    m_queryTimeout.setInterval(ms);
}

void EngineProcess::onReadyRead() {
    if (m_state != State::Analyzing) return;
    // analysis frames are bare "info" lines outside the \n\n response
    // protocol; GtpClient is paused so nothing else drains these bytes.
    // Partial lines are carried over (review fix I5).
    m_lineBuffer += m_proc->readAllStandardOutput();
    int start = 0;
    for (;;) {
        const int nl = m_lineBuffer.indexOf('\n', start);
        if (nl < 0) break;
        const QByteArray line = m_lineBuffer.mid(start, nl - start);
        start = nl + 1;
        if (!line.startsWith("info")) continue;
        const AnalysisData d = AnalysisParser::parseInfo(
            QString::fromUtf8(line), m_cfg.type, m_boardSize);
        if (d.valid) {
            // normalize to black's perspective (review fix C2): the parser
            // keeps LZ's side-to-move value; convert here where the side
            // to move is known
            AnalysisData out = d;
            if (m_cfg.type == EngineConfig::LeelaZero
                && m_analysisQuery.color == Stone::White) {
                out.winrate = 1.0 - out.winrate;
                for (MoveCandidate& c : out.candidates)
                    c.winrate = 1.0 - c.winrate;
            }
            Q_EMIT analysisUpdate(out);
        }
    }
    m_lineBuffer.remove(0, start);
}

void EngineProcess::handleResponse(quint64 id, bool success, const QString& body) {
    Q_UNUSED(id);
    m_queryTimeout.stop();
    // position sync queue: send next command; analysis starts when drained
    if (!m_positionQueue.isEmpty()) {
        const QString next = m_positionQueue.takeFirst();
        if (m_positionQueue.isEmpty()) {
            // last sync command: start the analysis stream after it lands
            m_state = State::Stopping;
            query(m_nextId++, next);
            doStartAnalysis();
        } else {
            armQueryTimeout();
            query(m_nextId++, next);
        }
        return;
    }
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
