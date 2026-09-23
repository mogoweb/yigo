#pragma once
#include <QObject>
#include <QString>

#include "Game.h"
#include "EngineProcess.h"

struct PlayerConfig {
    enum Kind { Human, AI } kind = Human;
};

struct GameSetup {
    int boardSize = 19;
    double komi = 7.5;
    int handicap = 0;
    PlayerConfig black;
    PlayerConfig white;
};

// Play-mode state machine: Idle / HumanTurn / EngineThinking / GameOver.
// Drives turn rotation between the human side and the GTP engine (genmove).
// Engine crash only rolls the phase back to HumanTurn — never quits (spec §6).
class GameController : public QObject {
    Q_OBJECT
public:
    enum class Phase { Idle, HumanTurn, EngineThinking, GameOver };
    explicit GameController(QObject* parent = nullptr);
    ~GameController() override;

    void newGame(const GameSetup& setup);
    Game* game() const { return m_game; }
    // release ownership of the current Game without deleting it (review mode)
    // controller phase resets to Idle
    Game* detachGame();
    Phase phase() const { return m_phase; }
    Stone engineColor() const { return m_engineColor; }
    void attachEngine(EngineProcess* engine);   // not owned
    void detachEngine();
    Stone winner() const { return m_winner; }         // valid when GameOver
    QString endReason() const { return m_endReason; } // resign/two passes/engine error
    ScoreResult finalScore() const { return m_finalScore; }

public slots:
    // human plays a stone (or pass with (-1,-1)); illegal -> moveRejected
    bool humanPlay(QPoint pos);
    void resign();                       // side to move resigns
    bool undoInPlay();                   // roll back human+AI pair (Task 4)

signals:
    void phaseChanged(GameController::Phase phase);
    void moveRejected(const QString& reason);
    void gameOver(Stone winner, const QString& reason);

private slots:
    void onQueryFinished(quint64 id, bool success, const QString& body);
    void onEngineCrashed(int code);
    void onEngineError(const QString& msg);

private:
    void advanceTurn();
    void requestEngineMove();
    void engineFailed(const QString& reason);
    void endGame(Stone winner, const QString& reason);
    void scoreEnd();

    Game* m_game = nullptr;
    GameSetup m_setup;
    Phase m_phase = Phase::Idle;
    Stone m_engineColor = Stone::Empty;   // Empty = both sides are AI
    Stone m_winner = Stone::Empty;
    QString m_endReason;
    ScoreResult m_finalScore;
    EngineProcess* m_engine = nullptr;
    QMetaObject::Connection m_queryConn;
    QMetaObject::Connection m_crashConn;
    QMetaObject::Connection m_errConn;
    quint64 m_genmoveId = 0;
    bool m_movePending = false;
    bool m_lastWasPass = false;
    int m_genmoveRetries = 0;
};
