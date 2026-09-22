#include "MainWindow.h"

#include "BoardView.h"
#include "EnginePanel.h"
#include "EngineProcess.h"
#include "MainWindowLogic.h"
#include "SgfParser.h"

#include <QDockWidget>
#include <QFile>
#include <QFileDialog>
#include <QFileInfo>
#include <QKeyEvent>
#include <QLabel>
#include <QMenuBar>
#include <QMessageBox>
#include <QStatusBar>

MainWindow::MainWindow(QWidget* parent) : QMainWindow(parent) {
    setWindowTitle(tr("YiGo 弈境"));
    resize(800, 600);
    setupCentral();
    setupMenus();
    newGame(19);
}

void MainWindow::setupCentral() {
    m_boardView = new BoardView(this);
    setCentralWidget(m_boardView);
    connect(m_boardView, &BoardView::boardClicked, this, &MainWindow::onBoardClicked);
    m_moveLabel = new QLabel(this);
    m_turnLabel = new QLabel(this);
    statusBar()->addPermanentWidget(m_moveLabel);
    statusBar()->addPermanentWidget(m_turnLabel);
    statusBar()->showMessage(tr("Ready"), 2000);

    // engine dock + process
    m_enginePanel = new EnginePanel(this);
    auto* dock = new QDockWidget(tr("Engine"), this);
    dock->setWidget(m_enginePanel);
    addDockWidget(Qt::RightDockWidgetArea, dock);
    m_engine = new EngineProcess(this);
    connect(m_enginePanel, &EnginePanel::startRequested, this, &MainWindow::onEngineStart);
    connect(m_enginePanel, &EnginePanel::stopRequested, this, &MainWindow::onEngineStop);
    connect(m_engine, &EngineProcess::connected, this, &MainWindow::onEngineConnected);
    connect(m_engine, &EngineProcess::crashed, this, &MainWindow::onEngineCrashed);
    connect(m_engine, &EngineProcess::errorOccurred, this, &MainWindow::onEngineError);
    connect(m_engine, &EngineProcess::analysisUpdate, this, &MainWindow::onAnalysisUpdate);
}

void MainWindow::onEngineStart(const EngineConfig& cfg) {
    m_enginePanel->setStatus(tr("Starting..."));
    if (!m_engine->start(cfg)) {
        m_enginePanel->setStatus(tr("Start failed"), true);
    } else {
        m_enginePanel->setStatus(tr("Running"));
        m_enginePanel->setRunning(true);
    }
}

void MainWindow::onEngineStop() {
    m_engine->stop();
    m_enginePanel->setStatus(tr("Stopped"));
    m_enginePanel->setRunning(false);
    m_boardView->setAnalysisOverlay(nullptr);
}

void MainWindow::onEngineConnected() {
    m_enginePanel->setStatus(tr("Connected: %1").arg(m_engine->engineName()));
}

void MainWindow::onEngineCrashed(int) {
    // spec §6: main program survives; offer restart via panel Start
    m_enginePanel->setStatus(tr("Engine crashed — offline play continues"), true);
    m_enginePanel->setRunning(false);
    statusBar()->showMessage(tr("Engine crashed"), 4000);
}

void MainWindow::onEngineError(const QString& msg) {
    m_enginePanel->setStatus(msg, true);
    m_enginePanel->setRunning(false);
}

void MainWindow::onAnalysisUpdate(const AnalysisData& data) {
    m_lastAnalysis = data;
    m_boardView->setAnalysisOverlay(&m_lastAnalysis);
    const Stone toMove = m_game ? m_game->nextToPlay() : Stone::Black;
    const double w = toMove == Stone::Black ? data.winrate : 1.0 - data.winrate;
    statusBar()->showMessage(tr("Winrate %1%  Visits %2")
                                 .arg(int(w * 100)).arg(data.visits), 3000);
}

void MainWindow::setupMenus() {
    QMenu* file = menuBar()->addMenu(tr("&File"));
    QAction* openAct = file->addAction(tr("&Open..."), this, &MainWindow::onOpen);
    openAct->setShortcut(QKeySequence::Open);
    QAction* saveAct = file->addAction(tr("&Save As..."), this, &MainWindow::onSave);
    saveAct->setShortcut(QKeySequence::Save);
    file->addSeparator();
    QAction* quitAct = file->addAction(tr("E&xit"), this, &QWidget::close);
    quitAct->setShortcut(QKeySequence::Quit);

    QMenu* game = menuBar()->addMenu(tr("&Game"));
    QMenu* newMenu = game->addMenu(tr("&New"));
    connect(newMenu->addAction(tr("19 x 19")), &QAction::triggered,
            this, &MainWindow::onNewGame19);
    connect(newMenu->addAction(tr("13 x 13")), &QAction::triggered,
            this, &MainWindow::onNewGame13);
    connect(newMenu->addAction(tr("9 x 9")), &QAction::triggered,
            this, &MainWindow::onNewGame9);
    QAction* undoAct = game->addAction(tr("&Undo"), this, &MainWindow::onUndo);
    undoAct->setShortcut(QKeySequence::Undo);
    QAction* passAct = game->addAction(tr("&Pass"), this, &MainWindow::onPass);
    passAct->setShortcut(tr("P"));
}

void MainWindow::newGame(int size) {
    if (!confirmDiscard()) return;
    delete m_game;
    m_game = new Game(size);
    m_currentFile.clear();
    m_dirty = false;
    m_boardView->setGame(m_game);
    setWindowTitle(tr("YiGo 弈境"));
    refreshStatus();
}

void MainWindow::onNewGame19() { newGame(19); }
void MainWindow::onNewGame13() { newGame(13); }
void MainWindow::onNewGame9()  { newGame(9); }

void MainWindow::onBoardClicked(QPoint pos) {
    if (!m_game) return;
    QString hint;
    if (!MainWindowLogic::handleBoardClick(*m_game, pos, &hint)) {
        statusBar()->showMessage(hint, 2000);   // no modal per spec §6
        return;
    }
    markDirty();
    m_boardView->update();
    refreshStatus();
}

void MainWindow::refreshStatus() {
    m_moveLabel->setText(tr("Move %1").arg(m_game->currentNode()->moveNumber));
    const Stone who = m_game->nextToPlay();
    m_turnLabel->setText(who == Stone::Black ? tr("Black to play")
                                             : tr("White to play"));
}

void MainWindow::onUndo() {
    if (!m_game) return;
    if (MainWindowLogic::handleAction(*m_game, MainWindowLogic::Action::Undo)) {
        markDirty();
        m_boardView->update();
        refreshStatus();
    }
}

void MainWindow::onPass() {
    if (!m_game) return;
    if (MainWindowLogic::handleAction(*m_game, MainWindowLogic::Action::Pass)) {
        markDirty();
        m_boardView->update();
        refreshStatus();
    }
}

void MainWindow::onPrev() {
    if (!m_game) return;
    if (MainWindowLogic::handleAction(*m_game, MainWindowLogic::Action::PrevMove)) {
        m_boardView->update();
        refreshStatus();
    }
}

void MainWindow::onNext() {
    if (!m_game) return;
    if (MainWindowLogic::handleAction(*m_game, MainWindowLogic::Action::NextMove)) {
        m_boardView->update();
        refreshStatus();
    }
}

void MainWindow::onFirst() {
    if (!m_game) return;
    if (MainWindowLogic::handleAction(*m_game, MainWindowLogic::Action::FirstMove)) {
        m_boardView->update();
        refreshStatus();
    }
}

void MainWindow::onLast() {
    if (!m_game) return;
    if (MainWindowLogic::handleAction(*m_game, MainWindowLogic::Action::LastMove)) {
        m_boardView->update();
        refreshStatus();
    }
}

void MainWindow::keyPressEvent(QKeyEvent* e) {
    switch (e->key()) {
    case Qt::Key_Left:  onPrev();  return;
    case Qt::Key_Right: onNext();  return;
    case Qt::Key_Home:  onFirst(); return;
    case Qt::Key_End:   onLast();  return;
    case Qt::Key_P:     onPass();  return;
    case Qt::Key_Z:
        if (e->modifiers() & Qt::ControlModifier) { onUndo(); return; }
        break;
    default: break;
    }
    QMainWindow::keyPressEvent(e);
}

void MainWindow::onOpen() {
    if (!confirmDiscard()) return;
    const QString path = QFileDialog::getOpenFileName(this, tr("Open SGF"), QString(),
                                                      tr("SGF files (*.sgf)"));
    if (path.isEmpty()) return;
    QFile f(path);
    if (!f.open(QIODevice::ReadOnly)) {
        QMessageBox::warning(this, tr("Open failed"), tr("Cannot read %1").arg(path));
        return;
    }
    QString err;
    Game* g = SgfParser::parse(QString::fromUtf8(f.readAll()), &err);
    if (!g) {
        QMessageBox::warning(this, tr("Open failed"), err);
        return;
    }
    delete m_game;
    m_game = g;
    m_currentFile = path;
    m_dirty = false;
    m_boardView->setGame(m_game);
    // jump to the last move so an opened game is visible immediately
    // (standard review-app behavior; use Home / Left to navigate back)
    MainWindowLogic::handleAction(*m_game, MainWindowLogic::Action::LastMove);
    setWindowTitle(tr("%1 - YiGo 弈境").arg(QFileInfo(path).fileName()));
    refreshStatus();
}

void MainWindow::onSave() {
    if (!m_game) return;
    const QString path = QFileDialog::getSaveFileName(this, tr("Save SGF"), m_currentFile,
                                                      tr("SGF files (*.sgf)"));
    if (path.isEmpty()) return;
    QFile f(path);
    if (!f.open(QIODevice::WriteOnly | QIODevice::Truncate)) {
        QMessageBox::warning(this, tr("Save failed"), tr("Cannot write %1").arg(path));
        return;
    }
    if (f.write(SgfParser::serialize(*m_game).toUtf8()) == -1) {
        QMessageBox::warning(this, tr("Save failed"), tr("Cannot write %1").arg(path));
        return;
    }
    m_currentFile = path;
    m_dirty = false;
    setWindowTitle(tr("%1 - YiGo 弈境").arg(QFileInfo(path).fileName()));
}

bool MainWindow::confirmDiscard() {
    if (!m_dirty) return true;   // clean (empty or just saved/loaded)
    const QMessageBox::StandardButton r = QMessageBox::question(
        this, tr("Discard current game?"),
        tr("The current game is not saved. Discard it?"),
        QMessageBox::Discard | QMessageBox::Cancel, QMessageBox::Cancel);
    return r == QMessageBox::Discard;
}
