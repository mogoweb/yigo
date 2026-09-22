#include <QCoreApplication>
#include <QFile>
#include <QStringList>
#include <QTextStream>
#include <cstdio>

#include "Board.h"
#include "Game.h"
#include "Rules.h"
#include "SgfParser.h"

static void printBoard(const Board& b) {
    static const char* glyph[] = {".", "X", "O"};
    for (int y = 0; y < b.size(); ++y) {
        std::printf("%2d ", b.size() - y);   // row number (1 at top)
        for (int x = 0; x < b.size(); ++x)
            std::printf("%s ", glyph[int(b.stoneAt(x, y))]);
        std::printf("\n");
    }
    std::printf("   ");
    for (int x = 0; x < b.size(); ++x)
        std::printf("%c ", "ABCDEFGHJKLMNOPQRSTUVWXYZ"[x]);   // skip I
    std::printf("\n");
}

int main(int argc, char** argv) {
    QCoreApplication app(argc, argv);
    const auto args = app.arguments();
    if (args.size() >= 3 && args[1] == "play" && args.size() >= 4) {
        Game g(args[2].toInt());
        Stone c = Stone::Black;
        for (int i = 3; i < args.size(); ++i) {
            const QString& m = args[i];
            if (m.compare("pass", Qt::CaseInsensitive) == 0) {
                std::printf("--- pass ---\n");
            } else if (m.size() >= 2) {
                int x = m[0].toUpper().toLatin1() - 'A';
                if (x > 8) --x;   // display letters skip I
                int y = g.boardSize() - m.mid(1).toInt();
                auto* n = g.play(QPoint(x, y), c);
                if (!n) {
                    std::printf("illegal: %s\n", qPrintable(m));
                    return 1;
                }
                c = Board::opponent(c);
                printBoard(g.board());
                std::printf("--- move %d ---\n", n->moveNumber);
            }
        }
        Rules r(g.rules());
        auto s = r.score(g.board(), g.blackCaptures(), g.whiteCaptures());
        std::printf("score: B %.1f  W %.1f  margin %.1f\n",
                    s.blackScore, s.whiteScore, s.blackMargin);
        return 0;
    }
    if (args.size() >= 3 && args[1] == "load") {
        QFile f(args[2]);
        if (!f.open(QIODevice::ReadOnly)) {
            std::printf("cannot open\n");
            return 1;
        }
        QString err;
        auto* g = SgfParser::parse(QString::fromUtf8(f.readAll()), &err);
        if (!g) {
            std::printf("parse error: %s\n", qPrintable(err));
            return 1;
        }
        std::printf("size=%d nodes=%d komi=%.1f\n", g->boardSize(),
                    g->tree().nodeCount(), g->rules().komi);
        printBoard(g->board());
        delete g;
        return 0;
    }
    std::printf("usage: yigo-cli play <size> <moves...> | load <file.sgf>\n");
    return 1;
}
