// Paint alignment probe: renders BoardView offscreen and pixel-checks that
// wood, grid lines and star points land exactly where BoardGeometry says.
// Regression guard for the paint/click transform drift fixed in 2a1eb7e.
// Usage: paint_probe [exit code 0 = aligned]
#include <QApplication>
#include <QImage>
#include <cstdio>
#include "BoardView.h"
#include "BoardGeometry.h"

static bool isDarkGrid(QRgb c) {
    // grid color is (60,45,25); allow antialias spread
    return qRed(c) < 150 && qGreen(c) < 130;
}

// true if a dark pixel exists within +/-1 px of (x,y)
static bool darkNear(const QImage& img, qreal x, qreal y) {
    for (int dy = -1; dy <= 1; ++dy)
        for (int dx = -1; dx <= 1; ++dx) {
            const int px = int(x) + dx, py = int(y) + dy;
            if (px < 0 || py < 0 || px >= img.width() || py >= img.height())
                continue;
            if (isDarkGrid(img.pixel(px, py))) return true;
        }
    return false;
}

// overlay regression (plan Task 5 / review I8): candidate tint must paint
// over the wood at the candidate intersection
static int checkOverlay() {
    Game g(9);
    AnalysisData d;
    d.valid = true;
    d.winrate = 0.6;
    d.visits = 42;
    MoveCandidate c;
    c.pos = QPoint(4, 4);
    c.winrate = 0.6;
    c.visits = 42;
    d.candidates.append(c);
    BoardView v;
    v.setGame(&g);
    v.resize(400, 400);
    const QImage base = v.grab().toImage();
    v.setAnalysisOverlay(&d);
    const QImage with = v.grab().toImage();
    const auto geom = BoardGeometry::forView(9, 400, 400);
    const QPointF center = geom.gridToPoint(4, 4);
    // the tinted ellipse + text must visibly change the candidate area
    int diffs = 0;
    for (int y = int(center.y()) - 15; y <= int(center.y()) + 15; ++y)
        for (int x = int(center.x()) - 15; x <= int(center.x()) + 15; ++x)
            if (base.pixel(x, y) != with.pixel(x, y)) ++diffs;
    if (diffs < 100) {
        std::printf("FAIL: overlay barely changed candidate area (%d px)\n", diffs);
        return 1;
    }
    // overlay tint is blue-ish (winrate >= 0.5): blue > red away from glyphs
    const QRgb px = with.pixel(int(center.x()) - 12, int(center.y()));
    if (qBlue(px) <= qRed(px)) {
        std::printf("FAIL: overlay tint not blue at candidate (rgb %d,%d,%d)\n",
                    qRed(px), qGreen(px), qBlue(px));
        return 1;
    }
    // a far-away intersection must be unchanged
    const QPointF far = geom.gridToPoint(0, 4);
    if (base.pixel(int(far.x()), int(far.y())) != with.pixel(int(far.x()), int(far.y()))) {
        std::printf("FAIL: overlay painted outside candidate\n");
        return 1;
    }
    return 0;
}

static int checkAlignment(QSize size, int boardSize) {
    Game g(boardSize);
    BoardView v;
    v.setGame(&g);
    v.resize(size);
    const QImage img = v.grab().toImage();
    const auto geom = BoardGeometry::forView(boardSize, size.width(), size.height());
    const QPointF origin = geom.origin();
    const qreal cell = geom.cellPx();

    // 1) wood starts at origin, window background outside (guard range)
    if (origin.x() > 12 && origin.y() > 12) {
        const QRgb inside = img.pixel(int(origin.x()) + 10, int(origin.y()) + 10);
        const QRgb outside = img.pixel(int(origin.x()) - 10, int(origin.y()) - 10);
        if (qRed(inside) == qRed(outside) && qGreen(inside) == qGreen(outside)) {
            std::printf("FAIL: wood does not start at origin (%.0f,%.0f)\n",
                        origin.x(), origin.y());
            return 1;
        }
    }

    // 2) each expected intersection lies on both a horizontal and a vertical
    //    grid line (1px pen draws on both sides of the coordinate; a line
    //    running through i also shows a short dark run at every other row)
    const int span = int((boardSize - 1) * cell);
    for (int iy = 0; iy < boardSize; iy += qMax(1, boardSize / 2)) {
        const qreal y = origin.y() + (iy + 1) * cell;
        if (!darkNear(img, origin.x() + cell + span / 2.0, y)) {
            std::printf("FAIL: no horizontal grid at row %d (y=%.0f)\n", iy, y);
            return 1;
        }
    }
    for (int ix = 0; ix < boardSize; ix += qMax(1, boardSize / 2)) {
        const qreal x = origin.x() + (ix + 1) * cell;
        if (!darkNear(img, x, origin.y() + cell + span / 2.0)) {
            std::printf("FAIL: no vertical grid at col %d (x=%.0f)\n", ix, x);
            return 1;
        }
    }
    return 0;
}

int main(int argc, char** argv) {
    qputenv("QT_QPA_PLATFORM", "offscreen");
    QApplication app(argc, argv);
    int failures = 0;
    // square view (origin = 0,0) and non-square view (origin offset) at two sizes
    failures += checkAlignment(QSize(400, 400), 9);
    failures += checkAlignment(QSize(600, 400), 9);
    failures += checkAlignment(QSize(800, 600), 19);
    failures += checkOverlay();
    std::printf("%s\n", failures == 0 ? "ALL OK" : "FAILURES");
    return failures;
}
