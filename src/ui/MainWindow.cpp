#include "MainWindow.h"

#include "BoardView.h"
#include "ChartWinrate.h"
#include "EnginePanel.h"
#include "EngineProcess.h"
#include "MainWindowLogic.h"
#include "NewGameDialog.h"
#include "SgfParser.h"

#include <QApplication>
#include <QDockWidget>
#include <QEvent>
#include <QFile>
#include <QFileDialog>
#include <QFileInfo>
#include <QKeyEvent>
#include <QLabel>
#include <QLocale>
#include <QMenuBar>
#include <QMessageBox>
#include <QStatusBar>
#include <QTranslator>

MainWindow::MainWindow(QWidget* parent) : QMainWindow(parent) {
    setWindowTitle(tr("YiGo 弈境"));
    resize(800, 600);
    setupCentral();
    setupMenus();
    // restore persisted state: game setup, engine config, window geometry
    const GameSetup setup = m_settings.gameSetup();
    startGame(setup);
    m_enginePanel->setConfig(m_settings.engineConfig());
    const QByteArray geo = m_settings.windowGeometry();
    if (!geo.isEmpty())
        restoreGeometry(geo);
}

void MainWindow::closeEvent(QCloseEvent* e) {
    m_settings.setWindowGeometry(saveGeometry());
    if (m_enginePanel)
        m_settings.setEngineConfig(m_enginePanel->config());
    if (m_engine)
        m_engine->stop();
    e->accept();
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

    // play controller
    m_controller = new GameController(this);
    connect(m_controller, &GameController::phaseChanged, this, &MainWindow::onPhaseChanged);
    connect(m_controller, &GameController::gameOver, this, &MainWindow::onGameOver);
    connect(m_controller, &GameController::moveRejected, this, [this](const QString& r) {
        statusBar()->showMessage(r, 2000);
    });

    // engine dock + process
    m_enginePanel = new EnginePanel(this);
    m_engineDock = new QDockWidget(QString(), this);
    m_engineDock->setWidget(m_enginePanel);
    addDockWidget(Qt::RightDockWidgetArea, m_engineDock);
    m_engine = new EngineProcess(this);
    connect(m_enginePanel, &EnginePanel::startRequested, this, &MainWindow::onEngineStart);
    connect(m_enginePanel, &EnginePanel::stopRequested, this, &MainWindow::onEngineStop);
    connect(m_engine, &EngineProcess::connected, this, &MainWindow::onEngineConnected);
    connect(m_engine, &EngineProcess::crashed, this, &MainWindow::onEngineCrashed);
    connect(m_engine, &EngineProcess::errorOccurred, this, &MainWindow::onEngineError);
    connect(m_engine, &EngineProcess::analysisUpdate, this, &MainWindow::onAnalysisUpdate);
    m_controller->attachEngine(m_engine);

    // review: bottom winrate chart dock + controller
    m_chart = new ChartWinrate(this);
    m_chartDock = new QDockWidget(QString(), this);
    m_chartDock->setWidget(m_chart);
    addDockWidget(Qt::BottomDockWidgetArea, m_chartDock);
    m_chartDock->hide();                   // review mode only
    m_review = new ReviewController(this);
    connect(m_review, &ReviewController::progressed, this, &MainWindow::onReviewProgress);
    connect(m_review, &ReviewController::finished, this, &MainWindow::onReviewFinished);
    connect(m_review, &ReviewController::blunderFound, this,
            [this](int mv, Stone side) {
                statusBar()->showMessage(
                    tr("Blunder at move %1 (%2)")
                        .arg(mv).arg(side == Stone::Black ? tr("Black") : tr("White")),
                    4000);
            });
    connect(m_chart, &ChartWinrate::moveClicked, this, &MainWindow::onChartClicked);
}

void MainWindow::onLanguageSelected(const QString& lang) {
    // i18n: persist and hot-swap translators; Qt then broadcasts
    // LanguageChange and every widget's changeEvent retranslates itself
    if (m_settings.language() == lang) return;
    m_settings.setLanguage(lang);
    const QList<QTranslator*> translators = QCoreApplication::instance()
                                                ->findChildren<QTranslator*>();
    for (QTranslator* t : translators) {
        QCoreApplication::removeTranslator(t);
        t->deleteLater();
    }
    QString effective = lang;
    if (effective.isEmpty())
        effective = QLocale::system().name().startsWith("zh")
                        ? QStringLiteral("zh_CN") : QStringLiteral("en");
    if (effective != QStringLiteral("en")) {
        auto* translator = new QTranslator(qApp);
        const QString name = QStringLiteral("yigo_") + effective;
        if (translator->load(name, QStringLiteral(YIGO_TRANSLATIONS_DIR))
            || translator->load(name,
                 QStringLiteral("/usr/share/yigo/translations")))
            qApp->installTranslator(translator);
        else
            translator->deleteLater();
        auto* qtTranslator = new QTranslator(qApp);
        if (qtTranslator->load(QStringLiteral("qtbase_") + effective,
                QStringLiteral("/usr/share/qt5/translations")))
            qApp->installTranslator(qtTranslator);
    }
    // removeTranslator/installTranslator post LanguageChange events; do one
    // pass now as well so the change feels immediate
    QEvent ev(QEvent::LanguageChange);
    QApplication::sendEvent(this, &ev);
}

void MainWindow::changeEvent(QEvent* e) {
    if (e->type() == QEvent::LanguageChange)
        retranslateUi();
    QMainWindow::changeEvent(e);
}

void MainWindow::onReview() {
    if (!m_game || !m_engine || !m_engine->isRunning()) {
        QMessageBox::warning(this, tr("Review"), tr("Start the engine first"));
        return;
    }
    m_reviewMode = true;
    m_chartDock->show();
    m_review->startReview(m_game, m_engine);
    statusBar()->showMessage(tr("Reviewing…"), 3000);
}

void MainWindow::onReviewProgress(int moveNumber) {
    m_chart->setCurve(&m_review->curve());
    m_chart->setCurrentMove(moveNumber);
    m_boardView->update();     // board follows the review cursor
    refreshStatus();
}

void MainWindow::onReviewFinished() {
    statusBar()->showMessage(m_review->isRunning()
                                 ? tr("Review aborted")
                                 : tr("Review complete"), 4000);
}

void MainWindow::onChartClicked(int moveNumber) {
    if (!m_reviewMode || !m_game) return;
    const auto ml = m_game->tree().mainLine();
    for (MoveNode* n : ml) {
        if (n->moveNumber == moveNumber) {
            m_game->goTo(n);
            m_chart->setCurrentMove(moveNumber);
            m_boardView->update();
            refreshStatus();
            return;
        }
    }
}

void MainWindow::onPhaseChanged(GameController::Phase phase) {
    switch (phase) {
    case GameController::Phase::Idle:
        m_turnLabel->setText(QString());
        break;
    case GameController::Phase::HumanTurn:
        m_turnLabel->setText(tr("Your turn"));
        break;
    case GameController::Phase::EngineThinking:
        m_turnLabel->setText(tr("Engine thinking…"));
        break;
    case GameController::Phase::GameOver:
        m_turnLabel->setText(tr("Game over"));
        break;
    }
    if (m_game)
        m_boardView->update();
}

void MainWindow::onGameOver(Stone winner, const QString& reason) {
    QString text;
    if (reason == "two passes") {
        const ScoreResult s = m_controller->finalScore();
        text = tr("%1 wins by %2 points (two passes)")
                   .arg(winner == Stone::Black ? tr("Black") : tr("White"))
                   .arg(qAbs(s.blackMargin));
    } else if (reason == "resign") {
        text = tr("%1 wins by resignation")
                   .arg(winner == Stone::Black ? tr("Black") : tr("White"));
    } else {
        text = tr("Game over: %1").arg(reason);
    }
    QMessageBox::information(this, tr("Game over"), text);
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
    // engine is ready: analyze the current position so the overlay goes live
    if (m_game) {
        AnalysisQuery q;
        q.color = m_game->nextToPlay();
        m_engine->analyzePosition(*m_game, q);
    }
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
    // AnalysisData.winrate is always black's perspective (spec invariant)
    const double w = toMove == Stone::Black ? data.winrate : 1.0 - data.winrate;
    statusBar()->showMessage(tr("Winrate %1%  Visits %2")
                                 .arg(int(w * 100)).arg(data.visits), 3000);
}

void MainWindow::setupMenus() {
    m_fileMenu = menuBar()->addMenu(QString());
    m_openAct = m_fileMenu->addAction(QString(), this, &MainWindow::onOpen);
    m_openAct->setShortcut(QKeySequence::Open);
    m_saveAct = m_fileMenu->addAction(QString(), this, &MainWindow::onSave);
    m_saveAct->setShortcut(QKeySequence::Save);
    m_fileMenu->addSeparator();
    m_quitAct = m_fileMenu->addAction(QString(), this, &QWidget::close);
    m_quitAct->setShortcut(QKeySequence::Quit);

    // language switcher: applies immediately via dynamic retranslation
    m_langMenu = m_fileMenu->addMenu(QString());
    QActionGroup* langGroup = new QActionGroup(m_langMenu);
    langGroup->setExclusive(true);
    const QString current = m_settings.language();
    const QString systemLang = QLocale::system().name().startsWith("zh")
                                   ? QStringLiteral("zh_CN") : QStringLiteral("en");
    m_langSystem = m_langMenu->addAction(QString());
    m_langSystem->setCheckable(true);
    m_langSystem->setChecked(current.isEmpty());
    connect(m_langSystem, &QAction::triggered, this,
            [this] { onLanguageSelected(QString()); });
    m_langZh = m_langMenu->addAction(QStringLiteral("中文"));
    m_langZh->setCheckable(true);
    m_langZh->setChecked(current == QStringLiteral("zh_CN")
                         || (current.isEmpty()
                             && systemLang == QStringLiteral("zh_CN")));
    connect(m_langZh, &QAction::triggered, this,
            [this] { onLanguageSelected(QStringLiteral("zh_CN")); });
    m_langEn = m_langMenu->addAction(QStringLiteral("English"));
    m_langEn->setCheckable(true);
    m_langEn->setChecked(current == QStringLiteral("en")
                         || (current.isEmpty()
                             && systemLang == QStringLiteral("en")));
    connect(m_langEn, &QAction::triggered, this,
            [this] { onLanguageSelected(QStringLiteral("en")); });

    m_gameMenu = menuBar()->addMenu(QString());
    m_newAct = m_gameMenu->addAction(QString(), this, &MainWindow::onNewGameDialog);
    m_newAct->setShortcut(QKeySequence::New);
    m_quickMenu = m_gameMenu->addMenu(QString());
    QAction* q19 = m_quickMenu->addAction(QString());
    connect(q19, &QAction::triggered, this, &MainWindow::onNewGame19);
    QAction* q13 = m_quickMenu->addAction(QString());
    connect(q13, &QAction::triggered, this, &MainWindow::onNewGame13);
    QAction* q9 = m_quickMenu->addAction(QString());
    connect(q9, &QAction::triggered, this, &MainWindow::onNewGame9);
    m_undoAct = m_gameMenu->addAction(QString(), this, &MainWindow::onUndo);
    m_undoAct->setShortcut(QKeySequence::Undo);
    m_passAct = m_gameMenu->addAction(QString(), this, &MainWindow::onPass);
    m_passAct->setShortcut(tr("P"));
    m_reviewAct = m_gameMenu->addAction(QString(), this, &MainWindow::onReview);
    m_reviewAct->setShortcut(tr("Ctrl+R"));
    retranslateUi();
}

void MainWindow::retranslateUi() {
    // all persistent texts re-applied through tr(); called on startup and
    // on every LanguageChange event so language switches apply instantly
    setWindowTitle(tr("YiGo 弈境"));
    m_fileMenu->setTitle(tr("&File"));
    m_openAct->setText(tr("&Open..."));
    m_saveAct->setText(tr("&Save As..."));
    m_quitAct->setText(tr("E&xit"));
    m_langMenu->setTitle(tr("&Language"));
    m_langSystem->setText(tr("Follow System"));
    // 中文 / English stay literal (self-describing language names)
    m_gameMenu->setTitle(tr("&Game"));
    m_newAct->setText(tr("&New..."));
    m_quickMenu->setTitle(tr("Quick &Start"));
    const QList<QAction*> quick = m_quickMenu->actions();
    if (quick.size() == 3) {
        quick[0]->setText(tr("19 x 19"));
        quick[1]->setText(tr("13 x 13"));
        quick[2]->setText(tr("9 x 9"));
    }
    m_undoAct->setText(tr("&Undo"));
    m_passAct->setText(tr("&Pass"));
    m_reviewAct->setText(tr("&Analyze Game"));
    m_engineDock->setWindowTitle(tr("Engine"));
    m_chartDock->setWindowTitle(tr("Winrate"));
    refreshStatus();
}

void MainWindow::newGame(int size) {
    GameSetup setup;
    setup.boardSize = size;
    startGame(setup);
}

void MainWindow::startGame(const GameSetup& setup) {
    if (!confirmDiscard()) return;
    m_currentFile.clear();
    m_dirty = false;
    // review holds main-line node pointers into the game being replaced —
    // stop the batch first (review Critical #1)
    m_review->stop();
    if (m_chart) m_chart->setCurve(nullptr);
    m_reviewMode = false;
    // the controller owns the Game — never delete through m_game (M4 C1 fix)
    m_controller->newGame(setup);
    m_game = m_controller->game();
    m_boardView->setGame(m_game);
    setWindowTitle(tr("YiGo 弈境"));
    refreshStatus();
}

void MainWindow::onNewGame19() { newGame(19); }
void MainWindow::onNewGame13() { newGame(13); }
void MainWindow::onNewGame9()  { newGame(9); }

void MainWindow::onNewGameDialog() {
    NewGameDialog dlg(this);
    dlg.setSetup(m_settings.gameSetup());
    if (dlg.exec() != QDialog::Accepted) return;   // confirmDiscard inside startGame
    m_settings.setGameSetup(dlg.setup());
    startGame(dlg.setup());
}

void MainWindow::onBoardClicked(QPoint pos) {
    if (!m_game || !m_controller) return;
    // play mode: route through the controller state machine
    if (m_controller->phase() != GameController::Phase::Idle
        && m_controller->phase() != GameController::Phase::GameOver) {
        if (!m_controller->humanPlay(pos)) return;   // rejection hint via signal
        markDirty();
        m_boardView->update();
        refreshStatus();
        return;
    }
    // review/edit path (no game started via controller or game over)
    QString hint;
    if (!MainWindowLogic::handleBoardClick(*m_game, pos, &hint)) {
        statusBar()->showMessage(hint, 2000);   // no modal per spec §6
        return;
    }
    markDirty();
    m_boardView->update();
    refreshStatus();
    // keep the live analysis in sync with the new position (review fix C1)
    if (m_engine && m_engine->isRunning()) {
        AnalysisQuery q;
        q.color = m_game->nextToPlay();
        m_engine->analyzePosition(*m_game, q);
    }
}

void MainWindow::refreshStatus() {
    if (!m_game || !m_moveLabel || !m_turnLabel) return;   // ctor order safety
    m_moveLabel->setText(tr("Move %1").arg(m_game->currentNode()->moveNumber));
    const Stone who = m_game->nextToPlay();
    m_turnLabel->setText(who == Stone::Black ? tr("Black to play")
                                             : tr("White to play"));
}

void MainWindow::onUndo() {
    if (!m_game) return;
    // play mode with controller: roll back the human+AI pair
    if (m_controller
        && (m_controller->phase() == GameController::Phase::HumanTurn
            || m_controller->phase() == GameController::Phase::EngineThinking)) {
        m_controller->undoInPlay();
        m_boardView->update();
        refreshStatus();
        return;
    }
    if (MainWindowLogic::handleAction(*m_game, MainWindowLogic::Action::Undo)) {
        markDirty();
        m_boardView->update();
        refreshStatus();
    }
}

void MainWindow::onPass() {
    if (!m_game) return;
    // C4 fix: in play mode route through the controller state machine
    if (m_controller && m_controller->phase() == GameController::Phase::HumanTurn) {
        m_controller->humanPlay(QPoint(-1, -1));
        m_boardView->update();
        refreshStatus();
        return;
    }
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
    // review holds pointers into the current game — stop before replacing
    m_review->stop();
    m_chart->setCurve(nullptr);
    m_reviewMode = false;
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
    // C2 fix: the controller owns the play-mode Game — detach it first so
    // the opened review game is owned by MainWindow alone
    m_controller->detachGame();
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
