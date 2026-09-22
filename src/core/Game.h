#pragma once
#include <QHash>
#include <QPoint>
#include <QVector>

#include "Board.h"
#include "GameTree.h"
#include "Rules.h"

class Game {
public:
    explicit Game(int boardSize = 19, const RulesConfig& cfg = RulesConfig{});

    int boardSize() const { return m_board.size(); }
    const Board& board() const { return m_board; }
    const RulesConfig& rules() const { return m_cfg; }
    GameTree& tree() { return m_tree; }
    const GameTree& tree() const { return m_tree; }
    MoveNode* currentNode() const { return m_current; }

    // play: if legal, create MoveNode under current node and advance,
    // returns the new node; returns nullptr if illegal
    MoveNode* play(QPoint pos, Stone color);
    bool undo();                          // step back to parent; false at root
    bool redo();                          // advance to children[0] (main); false if none
    bool redoVariant(int index);          // advance to children[index]
    void goTo(MoveNode* node);            // jump: replay pathTo(node)
    Stone nextToPlay() const;             // inferred from current node color

    // capture totals (for japanese scoring): black captured whites / vice versa
    int blackCaptures() const { return m_blackCaptures; }
    int whiteCaptures() const { return m_whiteCaptures; }

private:
    void rebuildBoard();
    void maybeSnapshot();
    void restoreSnapshotFor(MoveNode* target);

    Board m_board;
    RulesConfig m_cfg;
    GameTree m_tree;
    MoveNode* m_current = nullptr;
    int m_blackCaptures = 0;
    int m_whiteCaptures = 0;
    QHash<int, QVector<Stone>> m_snapshots;   // moveNumber -> grid
    static constexpr int SnapshotInterval = 50;
};
