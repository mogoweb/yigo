// Column-scan probe: prints every x column whose dark-pixel count inside the
// grid area exceeds a threshold — these are the painted vertical grid lines.
// Diagnostic companion to paint_probe; use it to eyeball where lines landed.
// Usage: colscan
#include <QApplication>
#include <QImage>
#include <cstdio>
#include "BoardView.h"
#include "BoardGeometry.h"

int main(int argc, char** argv) {
    qputenv("QT_QPA_PLATFORM", "offscreen");
    QApplication app(argc, argv);
    Game g(9);
    BoardView v;
    v.setGame(&g);
    v.resize(600, 400);
    const QImage img = v.grab().toImage();
    const auto geom = BoardGeometry::forView(9, 600.0, 400.0);
    const QPointF origin = geom.origin();
    const qreal cell = geom.cellPx();
    const int gridTop = int(origin.y() + cell);
    const int gridBottom = int(origin.y() + 9 * cell);
    std::printf("origin=(%.0f,%.0f) cell=%.0f grid span y=[%d..%d]\n",
                origin.x(), origin.y(), cell, gridTop, gridBottom);
    for (int x = 0; x < 600; ++x) {
        int dark = 0;
        for (int y = gridTop; y <= gridBottom; ++y) {
            const QRgb c = img.pixel(x, y);
            if (qRed(c) < 150 && qGreen(c) < 130) ++dark;
        }
        if (dark > 100)
            std::printf("x=%d dark=%d (expected col %.0f)\n",
                        x, dark, origin.x() + cell);
    }
    return 0;
}
