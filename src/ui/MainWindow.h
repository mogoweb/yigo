#pragma once
#include <QMainWindow>

#include "Game.h"

class BoardView;
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
};
