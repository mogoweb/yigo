#include "Board.h"
#include <QVector>

Board::Board(int size) : m_size(size) {
    Q_ASSERT(size >= 2 && size <= MaxSize);
    m_grid.fill(Stone::Empty, m_size * m_size);
}

int Board::size() const { return m_size; }

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
