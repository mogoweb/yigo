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
class EnginePanel;
class EngineProcess;
class QLabel;

class MainWindow : public QMainWindow {
    Q_OBJECT
public:
    explicit MainWindow(QWidget* parent = nullptr);

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
    QDockWidget* m_engineDock = nullptr;
    QDockWidget* m_chartDock = nullptr;

    Game* m_game = nullptr;
    BoardView* m_boardView = nullptr;
    QLabel* m_moveLabel = nullptr;
    QLabel* m_turnLabel = nullptr;
    QString m_currentFile;
    bool m_dirty = false;
    EngineProcess* m_engine = nullptr;
    EnginePanel* m_enginePanel = nullptr;
    AnalysisData m_lastAnalysis;
    GameController* m_controller = nullptr;
    ReviewController* m_review = nullptr;
    ChartWinrate* m_chart = nullptr;
    bool m_reviewMode = false;
    AppSettings m_settings;
};
