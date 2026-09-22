#pragma once
#include <QtGlobal>
#include <QHash>
#include <QPoint>
#include <QSet>
#include <QVector>

// Qt 5.11 has no built-in qHash(QPoint); needed for QSet/QHash key use.
inline uint qHash(const QPoint& p, uint seed = 0) Q_DECL_NOTHROW
{ return qHash(QPair<int,int>(p.x(), p.y()), seed); }

enum class Stone { Empty, Black, White };

class Board {
public:
    static constexpr int MaxSize = 25;

    explicit Board(int size = 19);
    int size() const { return m_size; }
    bool inBounds(int x, int y) const;
    Stone stoneAt(int x, int y) const;   // out of bounds: Q_ASSERT, release returns Empty
    static Stone opponent(Stone s);

    // place a stone: legal move writes and captures, returns true;
    // illegal (occupied/suicide/ko) returns false and board is unchanged.
    // capturedOut (if non-null) receives captured coordinates (may be empty).
    bool placeStone(int x, int y, Stone color, QVector<QPoint>* capturedOut = nullptr);
    bool isLegal(int x, int y, Stone color) const;      // judge only, no modification
    bool isSuicide(int x, int y, Stone color) const;    // assumes the point is empty
    void clear();
    void setSuperko(bool on) { m_superko = on; }
    quint64 positionHash() const { return m_hash; }
    void setupStone(int x, int y, Stone color);   // forced placement (no legality check), recompute hash

private:
    struct Chain { QVector<QPoint> stones; int liberties; };
    Chain chainAt(int x, int y) const;            // flood-fill same-color chain, count liberties
    int libertyCount(int x, int y) const;
    void removeChain(int x, int y, QVector<QPoint>* out);
    void recomputeHash();
    quint64 candidateHash(int x, int y, Stone color, const QVector<QPoint>& captured) const;

    QVector<Stone> m_grid;     // row-major: idx = y * size + x
    int m_size;
    QSet<quint64> m_history;   // hashes of all positions seen (incl. current), for superko
    quint64 m_prevHash = 0;
    quint64 m_hash = 0;
    bool m_superko = false;
    static quint64 s_zobrist[MaxSize * MaxSize][2];
    static bool s_zobristInit;
    static void initZobrist();
};
