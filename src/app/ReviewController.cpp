#include "ReviewController.h"
#include <QRegularExpression>

ReviewController::ReviewController(QObject* parent) : QObject(parent) {}

ReviewController::~ReviewController() {
    stop();
}

void ReviewController::startReview(Game* game, EngineProcess* engine) {
    stop();
    m_game = game;
    m_engine = engine;
    m_mainLine.clear();
    m_mainLine.append(game->tree().root());
    m_mainLine += game->tree().mainLine();
    m_curve.rebuild(m_mainLine);
    m_cursor = 0;
    m_running = true;
    m_paused = false;
    m_awaitReply = false;
    if (!m_connUpdate) {
        m_connUpdate = connect(m_engine, &EngineProcess::analysisUpdate, this,
                               &ReviewController::onAnalysisUpdate);
        m_connCrash = connect(m_engine, &EngineProcess::crashed, this,
                              &ReviewController::onCrashed);
        m_connErr = connect(m_engine, &EngineProcess::errorOccurred, this,
                            &ReviewController::onEngineError);
    }
    analyzeNext();
}

void ReviewController::pause() {
    m_paused = true;
}

void ReviewController::resume() {
    m_paused = false;
    analyzeNext();
}

void ReviewController::stop() {
    m_running = false;
    m_paused = false;
    m_awaitReply = false;
    if (m_engine && m_stateAnalyzing())
        m_engine->stopAnalysis();
}

bool ReviewController::m_stateAnalyzing() const {
    return m_engine && m_engine->isAnalyzing();
}

void ReviewController::analyzeNext() {
    if (!m_running || m_paused || m_awaitReply || !m_engine || !m_game) return;
    while (m_cursor < m_mainLine.size() && m_mainLine[m_cursor]->analysis.valid)
        ++m_cursor;                        // skip already-analyzed nodes
    if (m_cursor >= m_mainLine.size()) {
        m_running = false;
        Q_EMIT finished();
        return;
    }
    m_awaitReply = true;
    // review analyzes the position AFTER move k: jump the game there first
    m_game->goTo(m_mainLine[m_cursor]);
    AnalysisQuery q;
    q.color = m_mainLine[m_cursor]->color == Stone::Empty
                  ? Stone::Black : m_mainLine[m_cursor]->color;
    m_engine->analyzePosition(*m_game, q);
}

void ReviewController::onAnalysisUpdate(const AnalysisData& data) {
    if (!m_awaitReply || !m_running) return;
    m_awaitReply = false;
    if (m_cursor >= m_mainLine.size()) return;
    MoveNode* n = m_mainLine[m_cursor];
    n->analysis = data;
    m_curve.setPoint(n->moveNumber, data.winrate);
    checkBlunder(n);
    Q_EMIT progressed(n->moveNumber);
    ++m_cursor;
    analyzeNext();
}

void ReviewController::checkBlunder(const MoveNode* n) {
    // compare against the previous analyzed point on the curve
    const auto& pts = m_curve.points();
    double prev = -1.0;
    for (int i = pts.size() - 1; i >= 0; --i) {
        if (pts[i].moveNumber < n->moveNumber) { prev = pts[i].winrate; break; }
    }
    if (prev < 0 || n->color == Stone::Empty) return;
    const double gain = n->color == Stone::Black
                            ? (n->analysis.winrate - prev)
                            : (prev - n->analysis.winrate);
    if (gain < -m_curve.blunderThreshold())
        Q_EMIT blunderFound(n->moveNumber, n->color);
}

void ReviewController::jumpTo(int moveNumber) {
    if (!m_running || !m_engine) return;
    int idx = -1;
    for (int i = 0; i < m_mainLine.size(); ++i)
        if (m_mainLine[i]->moveNumber == moveNumber) { idx = i; break; }
    if (idx < 0) return;
    m_engine->stopAnalysis();       // abandon current stream
    m_awaitReply = false;
    m_cursor = idx;
    analyzeNext();                  // analyze target, then resume queue
}

void ReviewController::onCrashed(int) {
    m_running = false;
    m_awaitReply = false;
    Q_EMIT finished();              // aborted: caller distinguishes via isRunning
}

void ReviewController::onEngineError(const QString&) {
    m_running = false;
    m_awaitReply = false;
    Q_EMIT finished();
}
