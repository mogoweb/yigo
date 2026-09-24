#include <QApplication>
#include "MainWindow.h"

int main(int argc, char* argv[]) {
    // HiDPI: spec §1 integer-scale baseline (Qt 5.6+ attribute, must be set
    // before QApplication)
    QApplication::setAttribute(Qt::AA_EnableHighDpiScaling);
    QApplication app(argc, argv);
    app.setApplicationName("YiGo");
    app.setApplicationVersion("0.5.0");
    app.setOrganizationName("YiGo");
    app.setOrganizationDomain("yigo");   // stable default QSettings location
    MainWindow w;
    w.show();
    return app.exec();
}
