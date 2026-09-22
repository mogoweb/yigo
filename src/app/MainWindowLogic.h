#pragma once
#include <QString>
#include <QPoint>

#include "Game.h"

// Pure QtCore logic behind MainWindow: click/key handling on a Game.
struct MainWindowLogic {
    enum class Action { PrevMove, NextMove, FirstMove, LastMove, Pass, Undo };
    static bool handleBoardClick(Game& game, QPoint pos, QString* hint = nullptr);
    static bool handleAction(Game& game, Action a, QString* hint = nullptr);
};
