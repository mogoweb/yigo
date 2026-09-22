#pragma once
#include <QPoint>
#include <QString>
#include <QVector>

#include "Board.h"

struct MoveCandidate {
    QPoint pos{-1, -1};
    double winrate = 0.0;      // black's perspective 0..1
    int visits = 0;
};
struct AnalysisData {          // M3: filled by analysis parser
    bool valid = false;
    double winrate = 0.0;      // black's perspective 0..1
    double scoreLead = 0.0;    // black's lead in points
    int visits = 0;
    QVector<MoveCandidate> candidates;
};
struct MoveNode {
    QPoint pos{-1, -1};        // (-1,-1) = pass; root node the same
    Stone color = Stone::Empty;
    int moveNumber = 0;        // root is 0, +1 per move
    QString comment;
    AnalysisData analysis;
    QVector<MoveNode*> children;   // children[0] = main line
    MoveNode* parent = nullptr;
};

class GameTree {
public:
    GameTree();                        // creates root node
    ~GameTree();                       // recursively deletes whole tree (exclusive ownership)
    GameTree(const GameTree&) = delete;
    GameTree& operator=(const GameTree&) = delete;

    MoveNode* root() const { return m_root; }
    // append a child node (color/pos) under parent, returns the new node
    MoveNode* addChild(MoveNode* parent, Stone color, QPoint pos);
    // detach and delete child subtree; returns false if not found / is root
    bool removeChild(MoveNode* child);
    QVector<MoveNode*> mainLine() const;             // root-to-main-line-end path
    QVector<MoveNode*> pathTo(MoveNode* node) const; // root-to-node path
    int nodeCount() const;

private:
    static void deleteSubtree(MoveNode* n);
    static int countNodes(MoveNode* n);
    static QVector<MoveNode*> firstChildLine(MoveNode* from);

    MoveNode* m_root = nullptr;
};
