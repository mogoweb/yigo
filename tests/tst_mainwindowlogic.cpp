#include <QtTest>
#include "MainWindowLogic.h"
#include "SgfParser.h"

class TestMainWindowLogic : public QObject {
    Q_OBJECT
private slots:
    void legalClickPlays() {
        Game g(9);
        QString hint;
        QVERIFY(MainWindowLogic::handleBoardClick(g, QPoint(2, 2), &hint));
        QVERIFY(hint.isEmpty());
        QCOMPARE(g.board().stoneAt(2, 2), Stone::Black);
        QCOMPARE(g.currentNode()->moveNumber, 1);
    }
    void illegalClickHints() {
        Game g(9);
        QVERIFY(g.play(QPoint(2, 2), Stone::Black));
        QString hint;
        QVERIFY(!MainWindowLogic::handleBoardClick(g, QPoint(2, 2), &hint));  // occupied
        QVERIFY2(!hint.isEmpty(), "hint text required for illegal move");
        QCOMPARE(g.currentNode()->moveNumber, 1);   // board unchanged
    }
    void navigationActions() {
        Game g(9);
        QVERIFY(g.play(QPoint(2, 2), Stone::Black));
        QVERIFY(g.play(QPoint(5, 5), Stone::White));
        QVERIFY(MainWindowLogic::handleAction(g, MainWindowLogic::Action::PrevMove));
        QCOMPARE(g.currentNode()->moveNumber, 1);
        QVERIFY(MainWindowLogic::handleAction(g, MainWindowLogic::Action::Undo));
        QCOMPARE(g.currentNode()->moveNumber, 0);
        QVERIFY(MainWindowLogic::handleAction(g, MainWindowLogic::Action::NextMove));
        QCOMPARE(g.currentNode()->moveNumber, 1);
        QVERIFY(MainWindowLogic::handleAction(g, MainWindowLogic::Action::LastMove));
        QCOMPARE(g.currentNode()->moveNumber, 2);
        QVERIFY(MainWindowLogic::handleAction(g, MainWindowLogic::Action::FirstMove));
        QCOMPARE(g.currentNode()->moveNumber, 0);
    }
    void passAppendsNode() {
        Game g(9);
        QString hint;
        QVERIFY(MainWindowLogic::handleAction(g, MainWindowLogic::Action::Pass, &hint));
        QCOMPARE(g.currentNode()->moveNumber, 1);
        QCOMPARE(g.currentNode()->pos, QPoint(-1, -1));
    }
    void loadSgfAndNavigate() {
        QString err;
        auto* g = SgfParser::parse("(;GM[1]FF[4]SZ[9];B[cc];W[gg])", &err);
        QVERIFY2(g != nullptr, qPrintable(err));
        QVERIFY(MainWindowLogic::handleAction(*g, MainWindowLogic::Action::LastMove));
        QCOMPARE(g->currentNode()->moveNumber, 2);
        QVERIFY(MainWindowLogic::handleAction(*g, MainWindowLogic::Action::FirstMove));
        QCOMPARE(g->currentNode()->moveNumber, 0);
        delete g;
    }
};

QTEST_GUILESS_MAIN(TestMainWindowLogic)
#include "tst_mainwindowlogic.moc"
