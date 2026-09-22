#include "Board.h"
#include <QQueue>
#include <QRandomGenerator>
#include <QSet>
#include <QVector>

quint64 Board::s_zobrist[Board::MaxSize * Board::MaxSize][2] = {};
bool Board::s_zobristInit = false;

void Board::initZobrist() {
    if (s_zobristInit) return;
    QRandomGenerator rng(0x1BADB002u);
    for (auto& cell : s_zobrist)
        for (auto& v : cell) v = rng.generate64();
    s_zobristInit = true;
}

Board::Board(int size) : m_size(size) {
    Q_ASSERT(size >= 2 && size <= MaxSize);
    m_grid.fill(Stone::Empty, m_size * m_size);
    initZobrist();
    recomputeHash();
}

bool Board::inBounds(int x, int y) const {
    return x >= 0 && x < m_size && y >= 0 && y < m_size;
}

Stone Board::stoneAt(int x, int y) const {
    if (!inBounds(x, y)) return Stone::Empty;
    return m_grid[y * m_size + x];
}

Stone Board::opponent(Stone s) {
    switch (s) {
    case Stone::Black: return Stone::White;
    case Stone::White: return Stone::Black;
    default: return Stone::Empty;
    }
}

Board::Chain Board::chainAt(int x, int y) const {
    Chain chain;
    const Stone color = stoneAt(x, y);
    if (color == Stone::Empty) return chain;
    QSet<QPoint> visited;
    QQueue<QPoint> queue;
    queue.enqueue(QPoint(x, y));
    visited.insert(QPoint(x, y));
    int liberties = 0;
    QSet<QPoint> libertyPoints;
    while (!queue.isEmpty()) {
        const QPoint p = queue.dequeue();
        chain.stones.append(p);
        static const int dx[] = {1, -1, 0, 0};
        static const int dy[] = {0, 0, 1, -1};
        for (int i = 0; i < 4; ++i) {
            const int nx = p.x() + dx[i];
            const int ny = p.y() + dy[i];
            if (!inBounds(nx, ny)) continue;
            const QPoint np(nx, ny);
            const Stone ns = stoneAt(nx, ny);
            if (ns == Stone::Empty) {
                if (!libertyPoints.contains(np)) {
                    libertyPoints.insert(np);
                    ++liberties;
                }
            } else if (ns == color && !visited.contains(np)) {
                visited.insert(np);
                queue.enqueue(np);
            }
        }
    }
    chain.liberties = liberties;
    return chain;
}

int Board::libertyCount(int x, int y) const {
    return chainAt(x, y).liberties;
}

void Board::removeChain(int x, int y, QVector<QPoint>* out) {
    const Stone color = stoneAt(x, y);
    if (color == Stone::Empty) return;
    const Chain chain = chainAt(x, y);
    for (const QPoint& p : chain.stones) {
        m_grid[p.y() * m_size + p.x()] = Stone::Empty;
        if (out) out->append(p);
    }
}

void Board::recomputeHash() {
    m_hash = 0;
    for (int y = 0; y < m_size; ++y) {
        for (int x = 0; x < m_size; ++x) {
            const Stone s = m_grid[y * m_size + x];
            if (s != Stone::Empty)
                m_hash ^= s_zobrist[y * MaxSize + x][s == Stone::Black ? 0 : 1];
        }
    }
}

quint64 Board::candidateHash(int x, int y, Stone color, const QVector<QPoint>& captured) const {
    quint64 h = m_hash;
    h ^= s_zobrist[y * MaxSize + x][color == Stone::Black ? 0 : 1];
    for (const QPoint& p : captured)
        h ^= s_zobrist[p.y() * MaxSize + p.x()][opponent(color) == Stone::Black ? 0 : 1];
    return h;
}

bool Board::isSuicide(int x, int y, Stone color) const {
    if (!inBounds(x, y)) return true;
    // any adjacent empty point → not suicide
    static const int dx[] = {1, -1, 0, 0};
    static const int dy[] = {0, 0, 1, -1};
    for (int i = 0; i < 4; ++i) {
        const int nx = x + dx[i];
        const int ny = y + dy[i];
        if (inBounds(nx, ny) && stoneAt(nx, ny) == Stone::Empty) return false;
    }
    // capture adjacent enemy chain with no liberties → not suicide
    for (int i = 0; i < 4; ++i) {
        const int nx = x + dx[i];
        const int ny = y + dy[i];
        if (!inBounds(nx, ny)) continue;
        if (stoneAt(nx, ny) == opponent(color) && libertyCount(nx, ny) == 1) return false;
    }
    // would our chain (with placed stone) have liberties? simulate: count liberties
    // of friendly neighbor chains excluding the point itself
    // If any friendly neighbor chain has >1 liberty → not suicide.
    // If all friendly neighbor chains have exactly 1 liberty (only this point) and
    // no captures/empties → suicide.
    for (int i = 0; i < 4; ++i) {
        const int nx = x + dx[i];
        const int ny = y + dy[i];
        if (!inBounds(nx, ny)) continue;
        if (stoneAt(nx, ny) == color && libertyCount(nx, ny) > 1) return false;
    }
    return true;
}

bool Board::isLegal(int x, int y, Stone color) const {
    if (!inBounds(x, y)) return false;
    if (stoneAt(x, y) != Stone::Empty) return false;
    if (isSuicide(x, y, color)) return false;

    // ko check: compute candidate hash via try-place simulation
    const Stone oc = opponent(color);
    QVector<QPoint> captured;
    // determine which enemy chains would be captured
    static const int dx[] = {1, -1, 0, 0};
    static const int dy[] = {0, 0, 1, -1};
    QSet<QPoint> capSet;
    for (int i = 0; i < 4; ++i) {
        const int nx = x + dx[i];
        const int ny = y + dy[i];
        if (!inBounds(nx, ny)) continue;
        if (stoneAt(nx, ny) == oc && libertyCount(nx, ny) == 1) {
            const Chain c = chainAt(nx, ny);
            for (const QPoint& p : c.stones) capSet.insert(p);
        }
    }
    const QVector<QPoint> caps = capSet.values().toVector();
    const quint64 h = candidateHash(x, y, color, caps);
    if (m_superko) {
        if (m_history.contains(h)) return false;
    } else {
        if (h == m_prevHash) return false;
    }
    return true;
}

bool Board::placeStone(int x, int y, Stone color, QVector<QPoint>* capturedOut) {
    if (!isLegal(x, y, color)) return false;
    m_grid[y * m_size + x] = color;
    static const int dx[] = {1, -1, 0, 0};
    static const int dy[] = {0, 0, 1, -1};
    const Stone oc = opponent(color);
    for (int i = 0; i < 4; ++i) {
        const int nx = x + dx[i];
        const int ny = y + dy[i];
        if (!inBounds(nx, ny)) continue;
        if (stoneAt(nx, ny) == oc && libertyCount(nx, ny) == 0)
            removeChain(nx, ny, capturedOut);
    }
    m_prevHash = m_hash;
    recomputeHash();
    m_history.insert(m_hash);
    return true;
}

void Board::setupStone(int x, int y, Stone color) {
    if (!inBounds(x, y)) return;
    m_grid[y * m_size + x] = color;
    recomputeHash();
}

void Board::clear() {
    m_grid.fill(Stone::Empty, m_size * m_size);
    m_history.clear();
    m_prevHash = 0;
    recomputeHash();
    m_history.insert(m_hash);
}
