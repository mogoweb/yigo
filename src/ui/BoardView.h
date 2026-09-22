#pragma once
#include <QWidget>

#include "BoardGeometry.h"
#include "Game.h"

class QPainter;

class BoardView : public QWidget {
    Q_OBJECT
public:
    explicit BoardView(QWidget* parent = nullptr);
    void setGame(Game* game);
    void setBoardSize(int size);
    void setAnalysisOverlay(const AnalysisData* data);   // nullptr = no overlay

signals:
    void boardClicked(QPoint pos);

protected:
    void paintEvent(QPaintEvent*) override;
    void mousePressEvent(QMouseEvent*) override;
    void resizeEvent(QResizeEvent*) override;

private:
    void updateGeometry();
    void drawWood(QPainter& p);
    void drawGrid(QPainter& p);
    void drawStars(QPainter& p);
    void drawCoords(QPainter& p);
    void drawStones(QPainter& p);
    void drawOverlay(QPainter& p);
    void drawLastMoveMark(QPainter& p);
    QVector<QPoint> starPoints(int size) const;

    Game* m_game = nullptr;
    BoardGeometry m_geom{19, 30.0};
    const AnalysisData* m_overlay = nullptr;
};
