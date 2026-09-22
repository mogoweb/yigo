#include "GameTree.h"

GameTree::GameTree() {
    m_root = new MoveNode();
    m_root->moveNumber = 0;
}

GameTree::~GameTree() {
    deleteSubtree(m_root);
}

void GameTree::deleteSubtree(MoveNode* n) {
    if (!n) return;
    for (MoveNode* c : n->children)
        deleteSubtree(c);
    delete n;
}

MoveNode* GameTree::addChild(MoveNode* parent, Stone color, QPoint pos) {
    if (!parent) return nullptr;
    auto* node = new MoveNode();
    node->pos = pos;
    node->color = color;
    node->moveNumber = parent->moveNumber + 1;
    node->parent = parent;
    parent->children.append(node);
    return node;
}

bool GameTree::removeChild(MoveNode* child) {
    if (!child || !child->parent) return false;   // null or root
    MoveNode* parent = child->parent;
    if (!parent->children.contains(child)) return false;
    parent->children.removeOne(child);
    deleteSubtree(child);
    return true;
}

QVector<MoveNode*> GameTree::firstChildLine(MoveNode* from) {
    QVector<MoveNode*> line;
    MoveNode* cur = from;
    while (!cur->children.isEmpty()) {
        cur = cur->children[0];
        line.append(cur);
    }
    return line;
}

QVector<MoveNode*> GameTree::mainLine() const {
    return firstChildLine(m_root);
}

QVector<MoveNode*> GameTree::pathTo(MoveNode* node) const {
    QVector<MoveNode*> path;
    MoveNode* cur = node;
    while (cur && cur != m_root) {
        path.append(cur);
        cur = cur->parent;
    }
    // reverse (hand-rolled swap: QVector::swap(i,j) is Qt 5.13+)
    for (int i = 0, j = path.size() - 1; i < j; ++i, --j) {
        MoveNode* tmp = path[i];
        path[i] = path[j];
        path[j] = tmp;
    }
    return path;
}

int GameTree::countNodes(MoveNode* n) {
    if (!n) return 0;
    int count = 1;
    for (MoveNode* c : n->children)
        count += countNodes(c);
    return count;
}

int GameTree::nodeCount() const {
    return countNodes(m_root);
}
