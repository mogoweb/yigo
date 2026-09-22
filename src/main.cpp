#include <QApplication>
#include "MainWindow.h"

int main(int argc, char* argv[]) {
    QApplication app(argc, argv);
    app.setApplicationName("YiGo");
    app.setApplicationVersion("0.2.0");
    app.setOrganizationName("YiGo");
    MainWindow w;
    w.show();
    return app.exec();
}
