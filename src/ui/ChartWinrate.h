#pragma once
#include <QWidget>

#include "WinrateCurve.h"

// Winrate chart: X = move number, Y = black's winrate (0..1, up = black).
// Segments colored by black's gain (blue up / red down, spec §5); blunder
// points get an orange ring; the current move has a vertical dashed line.
// Clicking emits the nearest move number.
class ChartWinrate : public QWidget {
    Q_OBJECT
public:
    explicit ChartWinrate(QWidget* parent = nullptr);
    void setCurve(const WinrateCurve* curve);      // not owned
    void setCurrentMove(int moveNumber);

signals:
    void moveClicked(int moveNumber);

protected:
    void paintEvent(QPaintEvent*) override;
    void mousePressEvent(QMouseEvent*) override;

private:
    QPointF pointToPixel(int moveNumber, double winrate) const;
    int pixelToMove(QPointF p) const;

    const WinrateCurve* m_curve = nullptr;
    int m_current = -1;
    qreal m_marginLeft = 30.0, m_marginRight = 10.0, m_marginTop = 10.0, m_marginBottom = 20.0;
};
