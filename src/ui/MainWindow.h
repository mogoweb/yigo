#pragma once
#include <QMainWindow>

#include "AppSettings.h"
#include "EngineConfig.h"
#include "GameController.h"
#include "GameTree.h"
#include "Game.h"
#include "ReviewController.h"

class BoardView;
class ChartWinrate;
class EngineDialog;
class EnginePanel;
class EngineProcess;
class QLabel;

class MainWindow : public QMainWindow {
    Q_OBJECT
public:
    // settings injection for probes/tests; nullptr = default user-scope settings
    explicit MainWindow(AppSettings* settings = nullptr, QWidget* parent = nullptr);

public:
    Game* game() const { return m_game; }   // read-only access for probes

protected:

protected:
    void keyPressEvent(QKeyEvent* e) override;
    void closeEvent(QCloseEvent* e) override;
    void changeEvent(QEvent* e) override;

private slots:
    void onBoardClicked(QPoint pos);
    void onNewGame19();
    void onNewGame13();
    void onNewGame9();
    void onNewGameDialog();
    void onOpen();
    void onSave();
    void onUndo();
    void onPass();
    void onPrev();
    void onNext();
    void onFirst();
    void onLast();
    void onEngineStart(const EngineConfig& cfg);
    void onEngineStop();
    void onEngineSettings();
    void onStrengthChanged(int seconds);
    void onEngineConnected();
    void onEngineCrashed(int code);
    void onEngineError(const QString& msg);
    void onAnalysisUpdate(const AnalysisData& data);
    void onPhaseChanged(GameController::Phase phase);
    void onGameOver(Stone winner, const QString& reason);
    void onReview();
    void onReviewProgress(int moveNumber);
    void onReviewFinished();
    void onChartClicked(int moveNumber);
    void onLanguageSelected(const QString& lang);

private:
    void setupMenus();
    void setupCentral();
    void retranslateUi();
    void refreshStatus();
    void updateEngineLabel();
    void newGame(int size);
    void startGame(const GameSetup& setup);
    bool confirmDiscard();
    void markDirty() { m_dirty = true; }

    // persistent widgets/actions retranslated on language change
    QMenu* m_fileMenu = nullptr;
    QMenu* m_langMenu = nullptr;
    QMenu* m_gameMenu = nullptr;
    QMenu* m_quickMenu = nullptr;
    QAction* m_openAct = nullptr;
    QAction* m_saveAct = nullptr;
    QAction* m_quitAct = nullptr;
    QAction* m_newAct = nullptr;
    QAction* m_undoAct = nullptr;
    QAction* m_passAct = nullptr;
    QAction* m_reviewAct = nullptr;
    QAction* m_langSystem = nullptr;
    QAction* m_langZh = nullptr;
    QAction* m_langEn = nullptr;
    QDockWidget* m_chartDock = nullptr;
    QAction* m_engineAct = nullptr;

    Game* m_game = nullptr;
    BoardView* m_boardView = nullptr;
    QLabel* m_moveLabel = nullptr;
    QLabel* m_turnLabel = nullptr;
    QLabel* m_engineLabel = nullptr;   // engine state, visible without the dialog
    // last engine state for the status-bar label (retranslation needs it)
    enum class EngineUiState { Stopped, Starting, Running, Crashed, Error };
    EngineUiState m_engineState = EngineUiState::Stopped;
    QString m_currentFile;
    bool m_dirty = false;
    EngineProcess* m_engine = nullptr;
    EngineDialog* m_engineDialog = nullptr;   // owns the panel; single instance
    EnginePanel* m_enginePanel = nullptr;
    AnalysisData m_lastAnalysis;
    GameController* m_controller = nullptr;
    ReviewController* m_review = nullptr;
    ChartWinrate* m_chart = nullptr;
    bool m_reviewMode = false;
    AppSettings m_settings;
};
