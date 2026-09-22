#pragma once
#include <QtGlobal>
#include <QVector>

enum class Stone { Empty, Black, White };

class Board {
public:
    static constexpr int MaxSize = 25;

    explicit Board(int size = 19);
    int size() const;
    bool inBounds(int x, int y) const;
    Stone stoneAt(int x, int y) const;   // out of bounds: Q_ASSERT, release returns Empty
    static Stone opponent(Stone s);

private:
    QVector<Stone> m_grid;   // row-major: idx = y * size + x
    int m_size;
};
