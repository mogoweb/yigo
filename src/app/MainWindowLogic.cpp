#include "MainWindowLogic.h"

bool MainWindowLogic::handleBoardClick(Game& game, QPoint pos, QString* hint) {
    const Stone color = game.nextToPlay();
    if (!game.play(pos, color)) {
        if (hint) *hint = QStringLiteral("Illegal move");
        return false;
    }
    return true;
}

bool MainWindowLogic::handleAction(Game& game, Action a, QString* hint) {
    Q_UNUSED(hint);
    switch (a) {
    case Action::PrevMove: return game.undo();
    case Action::NextMove: return game.redo();
    case Action::FirstMove:
        game.goTo(game.tree().root());
        return true;
    case Action::LastMove: {
        MoveNode* n = game.currentNode();
        while (!n->children.isEmpty())
            n = n->children[0];
        game.goTo(n);
        return true;
    }
    case Action::Pass:
        return game.play(QPoint(-1, -1), game.nextToPlay()) != nullptr;
    case Action::Undo: return game.undo();
    }
    return false;
}
