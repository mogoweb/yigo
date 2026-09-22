#include <QtTest>
#include "AnalysisParser.h"

class TestAnalysisParser : public QObject {
    Q_OBJECT
private slots:
    void kataInfoStandard() {
        const auto d = AnalysisParser::parseInfo(
            "info move D4 visits 120 winrate 0.5123 scoreLead 1.2 utility 0.1 order 0",
            EngineConfig::KataGo, 19);
        QVERIFY(d.valid);
        QCOMPARE(d.winrate, 0.5123);
        QCOMPARE(d.visits, 120);
        QCOMPARE(d.scoreLead, 1.2);
        QCOMPARE(d.candidates.size(), 1);
        // GTP row numbers count from the bottom: D4 = (col 3, y 19-4=15)
        QCOMPARE(d.candidates[0].pos, QPoint(3, 15));
    }
    void kataFieldOrderVariants() {
        // same fields, different order — must parse by key not position
        const auto d = AnalysisParser::parseInfo(
            "info winrate 0.7 visits 55 move Q16 scoreLead -0.5",
            EngineConfig::KataGo, 19);
        QVERIFY(d.valid);
        QCOMPARE(d.winrate, 0.7);
        QCOMPARE(d.visits, 55);
        QCOMPARE(d.scoreLead, -0.5);
        QCOMPARE(d.candidates[0].pos, QPoint(15, 3));   // Q=15, row 16 -> y=3
    }
    void kataMultipleCandidates() {
        const auto d = AnalysisParser::parseInfo(
            "info move D4 visits 100 winrate 0.55 scoreLead 0.3 "
            "info move Q16 visits 80 winrate 0.45 scoreLead -0.2",
            EngineConfig::KataGo, 19);
        QVERIFY(d.valid);
        QCOMPARE(d.candidates.size(), 2);
        QCOMPARE(d.candidates[0].pos, QPoint(3, 15));
        QCOMPARE(d.candidates[1].visits, 80);
        // top-level = first candidate
        QCOMPARE(d.winrate, 0.55);
    }
    void lzPercentWinrate() {
        // LZ: winrate is permyriad (5123 = 51.23%), no scoreLead, lowercase
        // letter+digit coords "d4" (column d + row 4 from bottom)
        const auto d = AnalysisParser::parseInfo(
            "info move d4 visits 200 winrate 5123 prior 1.2 order 0",
            EngineConfig::LeelaZero, 19);
        QVERIFY(d.valid);
        QCOMPARE(d.winrate, 0.5123);
        QCOMPARE(d.visits, 200);
        QCOMPARE(d.scoreLead, 0.0);   // LZ has no scoreLead
        QCOMPARE(d.candidates[0].pos, QPoint(3, 15));
    }
    void lzBlackPerspectiveRaw() {
        // LZ reports winrate for side to move; conversion to black's view
        // is the caller's job (it knows side to move). Parser keeps raw value.
        const auto d = AnalysisParser::parseInfo(
            "info move d4 visits 10 winrate 9000", EngineConfig::LeelaZero, 19);
        QCOMPARE(d.winrate, 0.9);
    }
    void nonInfoLineRejected() {
        const auto d = AnalysisParser::parseInfo("= play ok", EngineConfig::KataGo, 19);
        QVERIFY(!d.valid);
    }
    void passMoveParsing() {
        QCOMPARE(AnalysisParser::parseMove("pass", 9), QPoint(-1, -1));
        QCOMPARE(AnalysisParser::parseMove("= pass", 9), QPoint(-1, -1));
        QCOMPARE(AnalysisParser::parseMove("resign", 9), QPoint(-2, -2));
    }
    void parseMoveLetters() {
        // GTP rows count from bottom: D4 = (3, 15) on 19x19
        QCOMPARE(AnalysisParser::parseMove("D4", 19), QPoint(3, 15));
        QCOMPARE(AnalysisParser::parseMove("Q16", 19), QPoint(15, 3));
        QCOMPARE(AnalysisParser::parseMove("A1", 19), QPoint(0, 18));
        QCOMPARE(AnalysisParser::parseMove("T19", 19), QPoint(18, 0));
    }
};

QTEST_GUILESS_MAIN(TestAnalysisParser)
#include "tst_analysisparser.moc"
