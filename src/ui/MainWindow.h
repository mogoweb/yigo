#pragma once
#include <QMainWindow>

#include "EngineConfig.h"
#include "GameTree.h"
#include "Game.h"

class BoardView;
class EnginePanel;
class EngineProcess;
class QLabel;

class MainWindow : public QMainWindow {
    Q_OBJECT
public:
    explicit MainWindow(QWidget* parent = nullptr);

protected:
    void keyPressEvent(QKeyEvent* e) override;

private slots:
    void onBoardClicked(QPoint pos);
    void onNewGame19();
    void onNewGame13();
    void onNewGame9();
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

private:
    void setupMenus();
    void setupCentral();
    void refreshStatus();
    void newGame(int size);
    bool confirmDiscard();
    void markDirty() { m_dirty = true; }

    Game* m_game = nullptr;
    BoardView* m_boardView = nullptr;
    QLabel* m_moveLabel = nullptr;
    QLabel* m_turnLabel = nullptr;
    QString m_currentFile;
    bool m_dirty = false;
    EngineProcess* m_engine = nullptr;
    EnginePanel* m_enginePanel = nullptr;
    AnalysisData m_lastAnalysis;
};
