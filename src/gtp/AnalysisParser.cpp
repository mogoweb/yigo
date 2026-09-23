#include "AnalysisParser.h"
#include <QStringList>

namespace {
// inverse of letterToCol: grid column -> GTP display letter (skips I)
QChar colToLetter(int x) {
    if (x < 0 || x > 24) return QChar();
    return QChar(static_cast<char>('A' + x + (x >= 8 ? 1 : 0)));
}

// GTP display letter -> column (skips I); 'A'->0
int letterToCol(QChar c) {
    const char ch = c.toUpper().toLatin1();
    if (ch >= 'A' && ch <= 'H') return ch - 'A';
    if (ch >= 'J' && ch <= 'T') return ch - 'A' - 1;
    return -1;
}
// display row number (1 = bottom) -> internal y (0 = top)
int numberToRow(int n, int boardSize) { return boardSize - n; }

// lowercase SGF coord pair ("dd") -> (x,y)
QPoint sgfToPos(const QString& s, int boardSize) {
    if (s.size() < 2) return QPoint(-1, -1);
    const int x = s[0].toLatin1() - 'a';
    const int y = s[1].toLatin1() - 'a';
    if (x < 0 || x >= boardSize || y < 0 || y >= boardSize) return QPoint(-1, -1);
    return QPoint(x, y);
}

bool isKey(const QString& t) {
    return t == "move" || t == "visits" || t == "winrate"
        || t == "scoreLead" || t == "prior" || t == "order"
        || t == "utility" || t == "lcb" || t == "pv" || t == "weight";
}
} // namespace

AnalysisData AnalysisParser::parseInfo(const QString& line,
                                       EngineConfig::EngineType type,
                                       int boardSize) {
    AnalysisData d;
    if (!line.startsWith("info")) return d;   // valid=false

    // split into per-move chunks: "info move X ... [info move Y ...]"
    QStringList chunks;
    int start = 0;
    int idx = line.indexOf(" info", 1);
    while (idx >= 0) {
        chunks.append(line.mid(start, idx - start));
        start = idx + 1;   // skip the space, keep "info..."
        idx = line.indexOf(" info", idx + 5);
    }
    chunks.append(line.mid(start));

    bool first = true;
    for (const QString& chunk : chunks) {
        const QStringList tok = chunk.split(' ', QString::SkipEmptyParts);
        QHash<QString, QString> kv;
        for (int i = 0; i < tok.size(); ++i) {
            if (isKey(tok[i]) && i + 1 < tok.size())
                kv.insert(tok[i], tok[i + 1]);
        }
        MoveCandidate c;
        if (kv.contains("move")) {
            const QString mv = kv.value("move");
            if (mv.compare("pass", Qt::CaseInsensitive) == 0) {
                c.pos = QPoint(-1, -1);
            } else if (type == EngineConfig::KataGo && mv.size() >= 2
                       && mv[0].isLetter() && mv[1].isDigit()) {
                // display coords: "D4"
                const int x = letterToCol(mv[0]);
                const int n = mv.mid(1).toInt();
                c.pos = (x >= 0 && n >= 1 && n <= boardSize)
                            ? QPoint(x, numberToRow(n, boardSize)) : QPoint(-1, -1);
            } else {
                // LZ coords: lowercase letter+digit "d4" — column letter +
                // digit row counting from the bottom (same mapping as GTP
                // display coords, just lowercase)
                if (mv.size() >= 2 && mv[0].isLetter() && mv[1].isDigit()) {
                    const int x = letterToCol(mv[0]);
                    const int n = mv.mid(1).toInt();
                    c.pos = (x >= 0 && n >= 1 && n <= boardSize)
                                ? QPoint(x, numberToRow(n, boardSize)) : QPoint(-1, -1);
                } else {
                    c.pos = sgfToPos(mv.toLower().left(2), boardSize);
                }
            }
        }
        if (kv.contains("visits"))
            c.visits = kv.value("visits").toInt();
        if (kv.contains("winrate")) {
            double w = kv.value("winrate").toDouble();
            if (type == EngineConfig::LeelaZero && w > 1.0) w /= 10000.0;   // permyriad
            c.winrate = w;
        }
        if (first) {
            d.valid = true;
            d.visits = c.visits;
            d.winrate = c.winrate;
            if (kv.contains("scoreLead"))
                d.scoreLead = kv.value("scoreLead").toDouble();
        }
        d.candidates.append(c);
        first = false;
    }
    return d;
}

QPoint AnalysisParser::parseMove(const QString& body, int boardSize) {
    QString mv = body.trimmed();
    if (mv.startsWith('=')) mv = mv.mid(1).trimmed();
    if (mv.compare("pass", Qt::CaseInsensitive) == 0) return QPoint(-1, -1);
    if (mv.compare("resign", Qt::CaseInsensitive) == 0) return QPoint(-2, -2);
    if (mv.size() >= 2 && mv[0].isLetter() && mv[1].isDigit()) {
        // display coords "D4"
        const int x = letterToCol(mv[0]);
        const int n = mv.mid(1).toInt();
        if (x >= 0 && n >= 1 && n <= boardSize)
            return QPoint(x, numberToRow(n, boardSize));
        return QPoint(-1, -1);
    }
    // lowercase sgf pair "dd"
    return sgfToPos(mv.toLower(), boardSize);
}

QString AnalysisParser::posToGtp(QPoint pos, int boardSize) {
    if (pos.x() < 0 || pos.y() < 0) return QStringLiteral("pass");
    const QChar col = colToLetter(pos.x());
    const int row = boardSize - pos.y();   // GTP rows count from the bottom
    return QString(col) + QString::number(row);
}

QStringList AnalysisParser::positionCommands(const Game& game) {
    QStringList cmds;
    cmds << QString("boardsize %1").arg(game.boardSize());
    cmds << QStringLiteral("clear_board");
    // handicap / AB setup stones must reach the engine too (review I5)
    for (const auto& st : game.setupStones()) {
        if (st.second == Stone::Empty) continue;
        cmds << QString("play %1 %2")
                    .arg(st.second == Stone::Black ? "B" : "W")
                    .arg(posToGtp(st.first, game.boardSize()));
    }
    cmds << QString("komi %1").arg(game.rules().komi);
    // replay the path root -> current
    QVector<MoveNode*> path = game.tree().pathTo(game.currentNode());
    for (MoveNode* n : path) {
        if (n->color == Stone::Empty) continue;
        cmds << QString("play %1 %2")
                    .arg(n->color == Stone::Black ? "B" : "W")
                    .arg(posToGtp(n->pos, game.boardSize()));
    }
    return cmds;
}
