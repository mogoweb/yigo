// Click mapping probe: injects mouse events at the *painted* pixel position
// of known intersections and asserts boardClicked emits the matching grid
// coordinates. Regression guard for the click transform drift fixed in
// 2a1eb7e (clicks landed ~2 intersections right of the cursor).
// Usage: click_probe [exit code 0 = all OK]
#include <QApplication>
#include <QMouseEvent>
#include <QtTest>
#include <QVector>
#include <cstdio>
#include "BoardView.h"
#include "BoardGeometry.h"

static int clickAndCheck(BoardView& v, const QPoint& pos,
                         QVector<QPoint>& received, int expectedX, int expectedY,
                         bool expectEvent) {
    QMouseEvent press(QEvent::MouseButtonPress, pos, Qt::LeftButton,
                      Qt::LeftButton, Qt::NoModifier);
    QApplication::sendEvent(&v, &press);
    QMouseEvent release(QEvent::MouseButtonRelease, pos, Qt::LeftButton,
                        Qt::LeftButton, Qt::NoModifier);
    QApplication::sendEvent(&v, &release);
    if (expectEvent) {
        const QPoint got = received.last();
        const bool ok = got == QPoint(expectedX, expectedY);
        std::printf("click (%d,%d) -> (%d,%d) %s\n", pos.x(), pos.y(),
                    got.x(), got.y(), ok ? "OK" : "FAIL");
        return ok ? 0 : 1;
    }
    const bool ok = received.isEmpty();   // no new event expected
    std::printf("off-board click (%d,%d) -> %s\n", pos.x(), pos.y(),
                ok ? "ignored OK" : "FAIL");
    return ok ? 0 : 1;
}
int main(int argc, char** argv) {
    qputenv("QT_QPA_PLATFORM", "offscreen");
    QApplication app(argc, argv);
    Game g(9);
    BoardView v;
    v.setGame(&g);
    v.resize(600, 400);
    v.show();
    QTest::qWait(100);   // let the first paint/resize settle
    const auto geom = BoardGeometry::forView(9, 600.0, 400.0);
    QVector<QPoint> received;
    QObject::connect(&v, &BoardView::boardClicked,
                     [&](QPoint p) { received.append(p); });
    int failures = 0;
    const int base = received.size();
    // tengen painted at (300,200); corner (0,0) painted at (140,40)
    failures += clickAndCheck(v, geom.gridToPoint(4, 4).toPoint(), received, 4, 4, true);
    failures += clickAndCheck(v, geom.gridToPoint(0, 0).toPoint(), received, 0, 0, true);
    // off-wood (inside widget, outside board area): no new event
    received.remove(received.size() - 2, 2);
    failures += clickAndCheck(v, QPoint(20, 200), received, 0, 0, false);
    Q_UNUSED(base);
    std::printf("%s\n", failures == 0 ? "ALL OK" : "FAILURES");
    return failures;
}
