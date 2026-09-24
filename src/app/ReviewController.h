#pragma once
#include <QObject>

#include "EngineProcess.h"
#include "Game.h"
#include "WinrateCurve.h"

// Batch review: analyzes the game's main line move-by-move (serial), filling
// WinrateCurve and each MoveNode's analysis. Pause/resume, jump (abandon
// current analysis, analyze target, resume queue), crash-safe (batch stops,
// completed moves kept).
class ReviewController : public QObject {
    Q_OBJECT
public:
    explicit ReviewController(QObject* parent = nullptr);
    ~ReviewController() override;
    void startReview(Game* game, EngineProcess* engine);
    void pause();
    void resume();
    void stop();
    bool isRunning() const { return m_running; }
    int cursor() const { return m_cursor; }               // moves analyzed
    const WinrateCurve& curve() const { return m_curve; }
    // abandon current analysis -> analyze move k -> resume queue from k+1
    void jumpTo(int moveNumber);

signals:
    void progressed(int moveNumber);          // one move analyzed
    void blunderFound(int moveNumber, Stone side);
    void finished();                          // done OR aborted (check isRunning)

private slots:
    void onAnalysisUpdate(const AnalysisData& data);
    void onCrashed(int code);
    void onEngineError(const QString& msg);

private:
    void analyzeNext();
    void checkBlunder(const MoveNode* n);
    bool m_stateAnalyzing() const;

    Game* m_game = nullptr;
    EngineProcess* m_engine = nullptr;
    QVector<MoveNode*> m_mainLine;
    WinrateCurve m_curve;
    int m_cursor = 0;
    bool m_running = false;
    bool m_paused = false;
    bool m_awaitReply = false;
    QMetaObject::Connection m_connUpdate;
    QMetaObject::Connection m_connCrash;
    QMetaObject::Connection m_connErr;
};
