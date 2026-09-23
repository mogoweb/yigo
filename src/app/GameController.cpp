#include "GameController.h"
#include "AnalysisParser.h"

GameController::GameController(QObject* parent) : QObject(parent) {}

GameController::~GameController() {
    detachEngine();
    delete m_game;
}

void GameController::attachEngine(EngineProcess* engine) {
    detachEngine();
    m_engine = engine;
    if (!m_engine) return;
    m_queryConn = connect(m_engine, &EngineProcess::queryFinished, this,
                          &GameController::onQueryFinished);
    m_crashConn = connect(m_engine, &EngineProcess::crashed, this,
                          &GameController::onEngineCrashed);
    m_errConn = connect(m_engine, &EngineProcess::errorOccurred, this,
                        &GameController::onEngineError);
}

void GameController::detachEngine() {
    if (!m_engine) return;
    disconnect(m_queryConn);
    disconnect(m_crashConn);
    disconnect(m_errConn);
    m_engine = nullptr;
}

void GameController::newGame(const GameSetup& setup) {
    delete m_game;
    m_setup = setup;
    RulesConfig cfg;
    cfg.komi = setup.komi;
    cfg.handicap = setup.handicap;
    m_game = new Game(setup.boardSize, cfg);
    if (setup.handicap > 0) m_game->setupHandicap(setup.handicap);
    m_winner = Stone::Empty;
    m_endReason.clear();
    m_finalScore = ScoreResult();
    m_movePending = false;
    m_lastWasPass = false;
    m_genmoveRetries = 0;
    const bool blackAI = setup.black.kind == PlayerConfig::AI;
    const bool whiteAI = setup.white.kind == PlayerConfig::AI;
    if (blackAI && whiteAI) m_engineColor = Stone::Empty;
    else if (blackAI) m_engineColor = Stone::Black;
    else if (whiteAI) m_engineColor = Stone::White;
    else m_engineColor = Stone::Empty;   // two humans: no engine moves
    m_phase = Phase::Idle;
    Q_EMIT phaseChanged(m_phase);
    advanceTurn();
}

Game* GameController::detachGame() {
    Game* g = m_game;
    m_game = nullptr;
    m_movePending = false;
    m_phase = Phase::Idle;
    Q_EMIT phaseChanged(m_phase);
    return g;
}

bool GameController::humanPlay(QPoint pos) {
    if (!m_game || m_phase != Phase::HumanTurn || m_movePending) {
        Q_EMIT moveRejected(QStringLiteral("Not your turn"));
        return false;
    }
    const Stone toMove = m_game->nextToPlay();
    // only the configured human side may play
    const PlayerConfig::Kind kind =
        toMove == Stone::Black ? m_setup.black.kind : m_setup.white.kind;
    if (kind != PlayerConfig::Human) {
        Q_EMIT moveRejected(QStringLiteral("Not your turn"));
        return false;
    }
    if (!m_game->play(pos, toMove)) {
        Q_EMIT moveRejected(QStringLiteral("Illegal move"));
        return false;
    }
    // C4 fix: human pass closes the two-pass game when the engine just passed
    if (pos.x() < 0) {
        if (m_lastWasPass) {
            scoreEnd();
            return true;
        }
        m_lastWasPass = true;
    } else {
        m_lastWasPass = false;
    }
    advanceTurn();
    return true;
}

void GameController::resign() {
    if (!m_game || m_phase != Phase::HumanTurn) return;
    endGame(Board::opponent(m_game->nextToPlay()), QStringLiteral("resign"));
}

void GameController::advanceTurn() {
    if (!m_game || m_phase == Phase::GameOver) return;
    const Stone toMove = m_game->nextToPlay();
    const PlayerConfig::Kind kind =
        toMove == Stone::Black ? m_setup.black.kind : m_setup.white.kind;
    const bool enginePlays = kind == PlayerConfig::AI && m_engine
                             && m_engine->isRunning()
                             && (m_engineColor == Stone::Empty
                                 || m_engineColor == toMove);
    if (enginePlays) {
        m_phase = Phase::EngineThinking;
        Q_EMIT phaseChanged(m_phase);
        requestEngineMove();
    } else {
        m_phase = Phase::HumanTurn;
        Q_EMIT phaseChanged(m_phase);
    }
}

void GameController::requestEngineMove() {
    if (m_movePending || !m_engine || !m_game) return;
    m_movePending = true;
    // C3 fix: the live analysis stream pauses the codec and swallows
    // non-info responses — interrupt it before sending genmove
    m_engine->stopAnalysis();
    // position sync first, then genmove; the genmove id is the LAST one so
    // its response can be paired via m_genmoveId
    const QStringList cmds = AnalysisParser::positionCommands(*m_game);
    m_engine->sendCommandSequence(cmds);
    m_genmoveId = m_engine->nextRequestId();
    m_engine->query(m_genmoveId,
                    QString("genmove %1")
                        .arg(m_game->nextToPlay() == Stone::Black ? "B" : "W"));
}

void GameController::onQueryFinished(quint64 id, bool success, const QString& body) {
    if (qgetenv("YIGO_GC_DEBUG") == "1")
        fprintf(stderr, "gc QF id=%llu genmoveId=%llu pending=%d body=[%s]\n",
                (unsigned long long)id, (unsigned long long)m_genmoveId,
                (int)m_movePending, body.toUtf8().constData());
    if (id != m_genmoveId || !m_movePending) return;
    m_movePending = false;
    if (!success) {
        engineFailed(QStringLiteral("genmove error"));
        return;
    }
    const QPoint mv = AnalysisParser::parseMove(body, m_game->boardSize());
    if (mv == QPoint(-2, -2)) {   // resign
        endGame(Board::opponent(m_game->nextToPlay()), QStringLiteral("resign"));
        return;
    }
    const Stone color = m_game->nextToPlay();
    if (mv.x() < 0) {   // pass
        m_game->play(QPoint(-1, -1), color);
        if (m_lastWasPass) {
            scoreEnd();
            return;
        }
        m_lastWasPass = true;
    } else if (m_game->play(mv, color)) {
        m_lastWasPass = false;
        m_genmoveRetries = 0;
    } else {
        // engine returned an illegal point: retry a few times, then give up
        if (++m_genmoveRetries < 3) {
            requestEngineMove();
            return;
        }
        endGame(Stone::Empty, QStringLiteral("engine error"));
        return;
    }
    advanceTurn();
}

void GameController::onEngineCrashed(int) {
    m_movePending = false;
    // spec §6: offline play continues — roll back to the human side
    if (m_phase == Phase::EngineThinking) {
        m_phase = Phase::HumanTurn;
        Q_EMIT phaseChanged(m_phase);
    }
}

void GameController::onEngineError(const QString&) {
    m_movePending = false;
    if (m_phase == Phase::EngineThinking) {
        m_phase = Phase::HumanTurn;
        Q_EMIT phaseChanged(m_phase);
    }
}

void GameController::engineFailed(const QString& reason) {
    m_movePending = false;
    endGame(Stone::Empty, reason);
}

void GameController::endGame(Stone winner, const QString& reason) {
    m_phase = Phase::GameOver;
    m_winner = winner;
    m_endReason = reason;
    Q_EMIT phaseChanged(m_phase);
    Q_EMIT gameOver(winner, reason);
}

void GameController::scoreEnd() {
    m_finalScore = Rules(m_game->rules()).score(
        m_game->board(), m_game->blackCaptures(), m_game->whiteCaptures());
    endGame(m_finalScore.blackMargin > 0 ? Stone::Black : Stone::White,
            QStringLiteral("two passes"));
}

bool GameController::undoInPlay() {
    if (!m_game || m_phase != Phase::HumanTurn || m_movePending) return false;
    const bool bothAI = m_setup.black.kind == PlayerConfig::AI
                        && m_setup.white.kind == PlayerConfig::AI;
    int steps = 0;
    if (bothAI) {
        steps = 1;
    } else {
        // roll back past the AI reply and the human move
        steps = m_game->currentNode()->moveNumber >= 2 ? 2
                                                       : m_game->currentNode()->moveNumber;
    }
    for (int i = 0; i < steps; ++i)
        m_game->undo();
    advanceTurn();
    return true;
}
