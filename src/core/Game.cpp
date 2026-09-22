#include "Game.h"

Game::Game(int boardSize, const RulesConfig& cfg)
    : m_board(boardSize), m_cfg(cfg), m_current(m_tree.root()) {}

MoveNode* Game::play(QPoint pos, Stone color) {
    if (!m_board.isLegal(pos.x(), pos.y(), color)) return nullptr;
    MoveNode* node = m_tree.addChild(m_current, color, pos);
    QVector<QPoint> captured;
    m_board.placeStone(pos.x(), pos.y(), color, &captured);
    if (color == Stone::Black)
        m_blackCaptures += captured.size();
    else if (color == Stone::White)
        m_whiteCaptures += captured.size();
    m_current = node;
    maybeSnapshot();
    return node;
}

bool Game::undo() {
    if (!m_current->parent) return false;
    m_current = m_current->parent;
    rebuildBoard();
    return true;
}

bool Game::redo() {
    return redoVariant(0);
}

bool Game::redoVariant(int index) {
    if (index < 0 || index >= m_current->children.size()) return false;
    m_current = m_current->children[index];
    rebuildBoard();
    return true;
}

void Game::goTo(MoveNode* node) {
    if (!node) return;
    m_current = node;
    rebuildBoard();
}

Stone Game::nextToPlay() const {
    if (m_current == m_tree.root()) return Stone::Black;
    return Board::opponent(m_current->color);
}

void Game::rebuildBoard() {
    restoreSnapshotFor(m_current);    // replay moves from the snapshot point up to current node
    // find the path from snapshot node (or root) to current
    QVector<MoveNode*> path;
    MoveNode* cur = m_current;
    MoveNode* base = nullptr;
    // locate the deepest snapshot at or before current
    int snapshotMove = -1;
    while (cur) {
        if (m_snapshots.contains(cur->moveNumber)) {
            snapshotMove = cur->moveNumber;
            base = cur;
            break;
        }
        path.append(cur);
        cur = cur->parent;
    }
    if (!base) {
        // replay everything from root; path currently ends at root
        base = m_tree.root();
    }
    // board = snapshot grid (or empty) + setup stones, then replay path
    m_board.clear();
    for (const auto& st : m_setupStones)
        m_board.setupStone(st.first.x(), st.first.y(), st.second);
    if (snapshotMove >= 0) {
        const QVector<Stone>& grid = m_snapshots[snapshotMove];
        // restore grid via setupStone to keep hash consistent
        for (int y = 0; y < m_board.size(); ++y)
            for (int x = 0; x < m_board.size(); ++x) {
                const Stone s = grid[y * m_board.size() + x];
                m_board.setupStone(x, y, s);
            }
        // ko history after restore is best-effort (positions recorded before
        // the snapshot are not reachable through normal play anyway)
    }
    // recompute captures by replaying
    int blackCaps = 0, whiteCaps = 0;
    // count captures along full path root->current (from move nodes),
    // for nodes after the snapshot base we replay and accumulate live;
    // for nodes before the snapshot we recompute from snapshot delta
    // Simplest correct approach: count captures for ALL moves on path by
    // simulating from scratch is expensive; instead accumulate from snapshot:
    // captures recorded implicitly — we replay from snapshot so live captures
    // accumulate for path nodes; for earlier nodes, derive by subtraction?
    // To stay simple and correct, always count captures by replaying from root
    // ONLY the capture counts (no board ops) — cheap.
    {
        QVector<QPoint> caps;
        MoveNode* n = m_current;
        // walk back collecting moves, then replay forward on a scratch count
        QVector<MoveNode*> moves;
        while (n && n != m_tree.root()) { moves.append(n); n = n->parent; }
        Board scratch(m_board.size());
        for (int i = moves.size() - 1; i >= 0; --i) {
            MoveNode* mv = moves[i];
            caps.clear();
            scratch.placeStone(mv->pos.x(), mv->pos.y(), mv->color, &caps);
            if (mv->color == Stone::Black) blackCaps += caps.size();
            else if (mv->color == Stone::White) whiteCaps += caps.size();
        }
    }
    m_blackCaptures = blackCaps;
    m_whiteCaptures = whiteCaps;
    // replay board moves from base to current
    for (int i = path.size() - 1; i >= 0; --i) {
        MoveNode* mv = path[i];
        m_board.placeStone(mv->pos.x(), mv->pos.y(), mv->color);
    }
    maybeSnapshot();
}

void Game::maybeSnapshot() {
    const int mn = m_current->moveNumber;
    if (mn > 0 && mn % SnapshotInterval == 0 && !m_snapshots.contains(mn)) {
        QVector<Stone> grid(m_board.size() * m_board.size());
        for (int y = 0; y < m_board.size(); ++y)
            for (int x = 0; x < m_board.size(); ++x)
                grid[y * m_board.size() + x] = m_board.stoneAt(x, y);
        m_snapshots.insert(mn, grid);
    }
}

void Game::restoreSnapshotFor(MoveNode* target) {
    // drop snapshots strictly after target to bound memory
    QHash<int, QVector<Stone>>::iterator it = m_snapshots.begin();
    while (it != m_snapshots.end()) {
        if (it.key() > target->moveNumber) it = m_snapshots.erase(it);
        else ++it;
    }
}
