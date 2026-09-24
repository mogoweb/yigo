#include "ChartWinrate.h"
#include <QMouseEvent>
#include <QPainter>

ChartWinrate::ChartWinrate(QWidget* parent) : QWidget(parent) {
    setMinimumSize(240, 120);
}

void ChartWinrate::setCurve(const WinrateCurve* curve) {
    m_curve = curve;
    update();
}

void ChartWinrate::setCurrentMove(int moveNumber) {
    m_current = moveNumber;
    update();
}

QPointF ChartWinrate::pointToPixel(int moveNumber, double winrate) const {
    const int maxMove = qMax(1, m_curve ? m_curve->count() - 1 : 1);
    const qreal w = width() - m_marginLeft - m_marginRight;
    const qreal h = height() - m_marginTop - m_marginBottom;
    const qreal x = m_marginLeft + w * (maxMove > 0 ? qreal(moveNumber) / maxMove : 0.0);
    const qreal y = m_marginTop + h * (1.0 - qBound(0.0, winrate, 1.0));
    return QPointF(x, y);
}

int ChartWinrate::pixelToMove(QPointF p) const {
    if (!m_curve || m_curve->count() < 1) return -1;
    const int maxMove = qMax(1, m_curve->count() - 1);
    const qreal w = width() - m_marginLeft - m_marginRight;
    const qreal frac = (p.x() - m_marginLeft) / w;
    const int mv = qRound(frac * maxMove);
    if (mv < 0 || mv > maxMove) return -1;
    return mv;
}

void ChartWinrate::paintEvent(QPaintEvent*) {
    QPainter p(this);
    p.setRenderHint(QPainter::Antialiasing, true);
    p.fillRect(rect(), Qt::white);
    // horizontal quarter grid
    p.setPen(QPen(QColor(220, 220, 220), 1.0));
    for (int i = 0; i <= 4; ++i) {
        const qreal y = m_marginTop + (height() - m_marginTop - m_marginBottom) * i / 4.0;
        p.drawLine(QPointF(m_marginLeft, y), QPointF(width() - m_marginRight, y));
    }
    if (!m_curve || m_curve->count() < 1) return;
    const auto& pts = m_curve->points();
    // segments colored by black's gain: blue = black gains, red = black loses
    for (int i = 1; i < pts.size(); ++i) {
        const double gain = pts[i].winrate - pts[i - 1].winrate;
        p.setPen(QPen(gain >= 0 ? QColor(60, 100, 220) : QColor(220, 60, 60), 2.0));
        p.drawLine(pointToPixel(pts[i - 1].moveNumber, pts[i - 1].winrate),
                   pointToPixel(pts[i].moveNumber, pts[i].winrate));
    }
    // points; blunders get an orange ring (spec §5)
    for (const auto& pt : pts) {
        if (pt.moveNumber == 0) continue;
        p.setPen(Qt::NoPen);
        p.setBrush(QColor(40, 40, 40));
        p.drawEllipse(pointToPixel(pt.moveNumber, pt.winrate), 3.0, 3.0);
        if (pt.blunder) {
            p.setPen(QPen(QColor(255, 140, 0), 2.0));
            p.setBrush(Qt::NoBrush);
            p.drawEllipse(pointToPixel(pt.moveNumber, pt.winrate), 5.5, 5.5);
        }
    }
    // current-move vertical line
    if (m_current >= 0 && !pts.isEmpty()) {
        const qreal x = pointToPixel(m_current, 0.5).x();
        p.setPen(QPen(Qt::black, 1.0, Qt::DashLine));
        p.drawLine(QPointF(x, m_marginTop), QPointF(x, height() - m_marginBottom));
    }
}

void ChartWinrate::mousePressEvent(QMouseEvent* e) {
    // Qt 5.11: localPos(); position() is 5.14+
    const int mv = pixelToMove(e->localPos());
    if (mv >= 0)
        Q_EMIT moveClicked(mv);
}
