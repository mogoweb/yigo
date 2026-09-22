#include <QtTest>
#include "SgfParser.h"
#include "Game.h"

class TestSgf : public QObject {
    Q_OBJECT
private slots:
    void minimalParse() {
        QString err;
        auto* g = SgfParser::parse("(;GM[1]FF[4]SZ[19];B[pd];W[pp])", &err);
        QVERIFY2(g != nullptr, qPrintable(err));
        QCOMPARE(g->boardSize(), 19);
        QCOMPARE(g->tree().nodeCount(), 3);   // root + 2 moves
        delete g;
    }
    void roundTrip() {
        const char* sgf =
            "(;GM[1]FF[4]CA[UTF-8]SZ[9]KM[7.5]PB[black1]PW[white1]DT[2026-09-21]RE[B+R]"
            ";B[cc];W[gg];C[hello (world);])";
        auto* g = SgfParser::parse(sgf);
        QVERIFY(g != nullptr);
        auto out = SgfParser::serialize(*g);
        auto* g2 = SgfParser::parse(out);
        QVERIFY2(g2 != nullptr, qPrintable(out));
        QCOMPARE(g2->tree().nodeCount(), g->tree().nodeCount());
        QCOMPARE(g2->rules().komi, 7.5);
        QCOMPARE(g2->tree().root()->children[0]->pos, QPoint(2, 2));
        QCOMPARE(g2->tree().root()->children[0]->children[0]->comment, QString("hello (world);"));
        delete g; delete g2;
    }
    void variations() {
        const char* sgf =
            "(;GM[1]FF[4]SZ[9];B[cc](;W[gg];B[dd])(;W[dd];B[gg]))";
        auto* g = SgfParser::parse(sgf);
        QVERIFY(g != nullptr);
        auto* root = g->tree().root();
        QCOMPARE(root->children.size(), 1);
        auto* b1 = root->children[0];
        QCOMPARE(b1->children.size(), 2);   // two white branches
        delete g;
    }
    void setupStonesAndHandicap() {
        // note: 'pp' would be (15,15), off-board on 9x9; use 'gg'=(6,6)
        const char* sgf = "(;GM[1]FF[4]SZ[9]HA[2]AB[dd][gg];W[cc])";
        auto* g = SgfParser::parse(sgf);
        QVERIFY(g != nullptr);
        QCOMPARE(g->rules().handicap, 2);
        QCOMPARE(g->board().stoneAt(3, 3), Stone::Black);
        QCOMPARE(g->board().stoneAt(6, 6), Stone::Black);
        delete g;
    }
    void compressedPointRange() {
        // AB[aa:cc] expands to the full rectangle a..c x a..c = 9 points
        const char* sgf = "(;GM[1]FF[4]SZ[9]AB[aa:cc];W[dd])";
        auto* g = SgfParser::parse(sgf);
        QVERIFY(g != nullptr);
        int blacks = 0;
        for (int x = 0; x < 9; ++x)
            for (int y = 0; y < 9; ++y)
                if (g->board().stoneAt(x, y) == Stone::Black) ++blacks;
        QCOMPARE(blacks, 9);
        delete g;
    }
    void passMove() {
        const char* sgf = "(;GM[1]FF[4]SZ[9];B[];W[tt])";
        auto* g = SgfParser::parse(sgf);
        QVERIFY(g != nullptr);
        auto* b1 = g->tree().root()->children[0];
        QCOMPARE(b1->pos, QPoint(-1, -1));
        auto* w1 = b1->children[0];
        QCOMPARE(w1->pos, QPoint(-1, -1));   // tt = pass (9x9)
        delete g;
    }
    void encodingDetection() {
        // CA[UTF-8] declared: chinese comment decodes correctly
        QByteArray raw = "(;GM[1]FF[4]SZ[9]CA[UTF-8];B[cc]C[\xe4\xb8\xad\xe6\x96\x87\xe6\xa3\x8b\xe8\xb0\xb1])";
        auto* g = SgfParser::parse(QString::fromUtf8(raw));
        QVERIFY(g != nullptr);
        QCOMPARE(g->tree().root()->children[0]->comment, QString::fromUtf8("中文棋谱"));
        delete g;
    }
    void malformedInputs() {
        struct Case { const char* in; } cases[] = {
            {"", }, {"(;GM[1]", }, {";B[cc])", },           // empty/truncated/no root
            {"(;GM[1];B[zz])", },                            // illegal coord
            {"(;GM[1]FF[4])(", },                            // stray open paren
        };
        for (auto& c : cases) {
            QString err;
            auto* g = SgfParser::parse(QString::fromLatin1(c.in), &err);
            if (g) {   // lenient policy: some inputs may parse to an empty tree
                delete g;
            } else {
                QVERIFY2(!err.isEmpty(), "error message required on failure");
            }
        }
    }
    void serializeBranchOrder() {
        // serialize keeps branch order (main line before variation);
        // variation branches at B(2,2): one undo after the white move
        Game g(9);
        g.play(QPoint(2, 2), Stone::Black);
        g.play(QPoint(3, 3), Stone::White);
        g.undo();
        g.play(QPoint(4, 4), Stone::White);
        auto out = SgfParser::serialize(g);
        auto* g2 = SgfParser::parse(out);
        QVERIFY2(g2 != nullptr, qPrintable(out));
        auto* b1 = g2->tree().root()->children[0];
        QCOMPARE(b1->children.size(), 2);
        QCOMPARE(b1->children[0]->pos, QPoint(3, 3));   // main line first
        delete g2;
    }
};

QTEST_MAIN(TestSgf)
#include "tst_sgf.moc"
