#include "MainWindow.h"
#include <QStatusBar>

MainWindow::MainWindow(QWidget* parent) : QMainWindow(parent) {
    setWindowTitle(tr("YiGo 弈境"));
    resize(800, 600);
    statusBar()->showMessage(tr("Ready"));
}
