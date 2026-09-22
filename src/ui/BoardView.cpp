#include "BoardView.h"
#include <QMouseEvent>
#include <QPainter>

BoardView::BoardView(QWidget* parent)
    : QWidget(parent), m_geom(19, 30.0) {
    setMinimumSize(400, 400);
}

void BoardView::setGame(Game* game) {
    m_game = game;
    if (m_game) setBoardSize(m_game->boardSize());
    update();
}

void BoardView::setBoardSize(int size) {
    m_geom = BoardGeometry(size, 30.0);
    update();
}

void BoardView::updateGeometry() {
    const int n = m_game ? m_game->boardSize() : 19;
    const qreal side = qMin(width(), height());
    m_geom = BoardGeometry(n, side / (n + 1));
}

void BoardView::resizeEvent(QResizeEvent*) {
    updateGeometry();
    update();
}

void BoardView::mousePressEvent(QMouseEvent* e) {
    updateGeometry();
    // Qt 5.11: localPos(); position() is 5.14+
    const QPointF pos = e->localPos();
    const int gx = m_geom.pointToGrid(pos);
    const int gy = m_geom.pointToGridY(pos);
    if (gx >= 0 && gy >= 0)
        emit boardClicked(QPoint(gx, gy));
}

QVector<QPoint> BoardView::starPoints(int size) const {
    QVector<QPoint> stars;
    QVector<int> edges;
    if (size == 19) edges = {3, 9, 15};
    else if (size == 13) edges = {3, 9};
    else if (size == 9) edges = {2, 6};
    else return stars;
    const int c = size / 2;
    for (int ey : edges)
        for (int ex : edges) {
            // 13/9: no edge-middle stars, corners + center only
            if (size != 19 && ((ex == c) != (ey == c))) continue;
            stars.append(QPoint(ex, ey));
        }
    if (size != 19)
        stars.append(QPoint(c, c));   // tengen for 13/9
    return stars;
}

void BoardView::drawWood(QPainter& p) {
    const QRectF area(0, 0, m_geom.boardPx(), m_geom.boardPx());
    QLinearGradient grad(area.topLeft(), area.bottomRight());
    grad.setColorAt(0.0, QColor(220, 179, 122));   // light kaya
    grad.setColorAt(1.0, QColor(196, 152, 92));    // darker kaya
    p.fillRect(area, grad);
}

void BoardView::drawGrid(QPainter& p) {
    const int n = m_game ? m_game->boardSize() : 19;
    p.setPen(QPen(QColor(60, 45, 25), 1.0));
    for (int i = 0; i < n; ++i) {
        const QPointF a = m_geom.gridToPoint(0, i);
        const QPointF b = m_geom.gridToPoint(n - 1, i);
        p.drawLine(a, QPointF(b.x(), a.y()));   // horizontal
        p.drawLine(a, QPointF(a.x(), b.y()));   // vertical
    }
    // outer border slightly thicker
    p.setPen(QPen(QColor(60, 45, 25), 2.0));
    p.drawRect(QRectF(m_geom.gridToPoint(0, 0), m_geom.gridToPoint(n - 1, n - 1)));
}

void BoardView::drawStars(QPainter& p) {
    const int n = m_game ? m_game->boardSize() : 19;
    p.setPen(Qt::NoPen);
    p.setBrush(QColor(60, 45, 25));
    const qreal r = m_geom.cellPx() * 0.09;
    for (const QPoint& s : starPoints(n))
        p.drawEllipse(m_geom.gridToPoint(s.x(), s.y()), r, r);
}

void BoardView::drawCoords(QPainter& p) {
    const int n = m_game ? m_game->boardSize() : 19;
    p.setPen(QColor(60, 45, 25));
    QFont f = font();
    f.setPointSizeF(qMax(6.0, f.pointSizeF() * 0.8));
    p.setFont(f);
    for (int i = 0; i < n; ++i) {
        // top letters
        const QRectF top((i + 1) * m_geom.cellPx() - m_geom.cellPx(), 0,
                         m_geom.cellPx() * 2, m_geom.cellPx());
        p.drawText(top, Qt::AlignCenter, QString(BoardGeometry::gridToDisplayX(i)));
        // left numbers (display row 1 at top = board row 0)
        const QRectF left(0, (i + 1) * m_geom.cellPx() - m_geom.cellPx(),
                          m_geom.cellPx(), m_geom.cellPx() * 2);
        p.drawText(left, Qt::AlignCenter, QString::number(n - i));
    }
}

void BoardView::drawStones(QPainter& p) {
    if (!m_game) return;
    const Board& b = m_game->board();
    const qreal r = m_geom.cellPx() * 0.47;
    for (int y = 0; y < b.size(); ++y) {
        for (int x = 0; x < b.size(); ++x) {
            const Stone s = b.stoneAt(x, y);
            if (s == Stone::Empty) continue;
            const QPointF c = m_geom.gridToPoint(x, y);
            QRadialGradient grad(c - QPointF(r * 0.3, r * 0.3), r * 1.4);
            if (s == Stone::Black) {
                grad.setColorAt(0.0, QColor(90, 90, 90));
                grad.setColorAt(0.4, QColor(25, 25, 25));
                grad.setColorAt(1.0, QColor(0, 0, 0));
            } else {
                grad.setColorAt(0.0, QColor(255, 255, 255));
                grad.setColorAt(0.5, QColor(235, 235, 230));
                grad.setColorAt(1.0, QColor(190, 190, 185));
            }
            p.setPen(QPen(QColor(40, 30, 20, 120), 1.0));
            p.setBrush(grad);
            p.drawEllipse(c, r, r);
        }
    }
}

void BoardView::drawLastMoveMark(QPainter& p) {
    if (!m_game) return;
    const MoveNode* cur = m_game->currentNode();
    if (!cur || !cur->parent || cur->pos.x() < 0) return;
    const QPointF c = m_geom.gridToPoint(cur->pos.x(), cur->pos.y());
    p.setPen(QPen(cur->color == Stone::Black ? QColor(255, 255, 255)
                                             : QColor(30, 30, 30), 2.0));
    p.setBrush(Qt::NoBrush);
    const qreal r = m_geom.cellPx() * 0.22;
    p.drawEllipse(c, r, r);
}

void BoardView::paintEvent(QPaintEvent*) {
    QPainter p(this);
    p.setRenderHint(QPainter::Antialiasing, true);
    // HiDPI: QPainter works in logical (device-independent) coords; Qt scales
    // by devicePixelRatioF() automatically. Keep all math in qreal.
    updateGeometry();
    p.translate((width() - m_geom.boardPx()) / 2.0,
                (height() - m_geom.boardPx()) / 2.0);
    drawWood(p);
    drawGrid(p);
    drawStars(p);
    drawCoords(p);
    drawStones(p);
    drawLastMoveMark(p);
}
