#include "SgfParser.h"
#include "SgfCoord.h"
#include <QTextCodec>
#include <QVector>

namespace {

struct SgfProp {
    QString id;
    QStringList values;
};

struct SgfNode {
    QVector<SgfProp> props;
};

// One SGF gametree element: nodes and nested sub-variations in encounter
// order (SGF allows interleaving; branch order matters for main-line-first)
struct Variation {
    QVector<SgfNode> nodes;
    QVector<Variation*> children;
    QVector<QPair<bool, int>> order;   // (isChild, index into nodes|children)
    ~Variation() { qDeleteAll(children); }
};

class Lexer {
public:
    Lexer(const QString& text) : m_text(text) {}

    bool atEnd() const { return m_pos >= m_text.size(); }
    int pos() const { return m_pos; }
    QChar peek() const { return m_pos < m_text.size() ? m_text[m_pos] : QChar(); }

    void skipWhitespace() {
        while (m_pos < m_text.size() && m_text[m_pos].isSpace()) ++m_pos;
    }

    bool consume(QChar c) {
        skipWhitespace();
        if (m_pos < m_text.size() && m_text[m_pos] == c) {
            ++m_pos;
            return true;
        }
        return false;
    }

    bool consumeIdent(QString& out) {
        skipWhitespace();
        int start = m_pos;
        while (m_pos < m_text.size() && m_text[m_pos].isLetter())
            ++m_pos;
        if (m_pos == start) return false;
        out = m_text.mid(start, m_pos - start);
        return true;
    }

    // reads a [...] value with escape handling; returns false without [...]
    bool consumeValue(QString& out) {
        skipWhitespace();
        if (m_pos >= m_text.size() || m_text[m_pos] != '[') return false;
        ++m_pos;
        out.clear();
        while (m_pos < m_text.size()) {
            const QChar c = m_text[m_pos];
            if (c == '\\') {
                ++m_pos;
                if (m_pos < m_text.size()) {
                    const QChar e = m_text[m_pos];
                    if (e == '\r' && m_pos + 1 < m_text.size() && m_text[m_pos+1] == '\n') {
                        ++m_pos;   // soft line break: drop entirely
                    } else if (e == '\r' || e == '\n') {
                        // soft line break: drop entirely
                    } else {
                        out.append(e);
                    }
                    ++m_pos;
                }
                continue;
            }
            if (c == ']') {
                ++m_pos;
                return true;
            }
            out.append(c);
            ++m_pos;
        }
        return false;   // unterminated value
    }

private:
    const QString& m_text;
    int m_pos = 0;
};

// Forward declarations for recursive descent
Variation* parseGameTree(Lexer& lex, QString& err);
bool parseNodeList(Lexer& lex, Variation* var, QString& err);
bool parseNode(Lexer& lex, SgfNode& node, QString& err);

bool parseNode(Lexer& lex, SgfNode& node, QString& err) {
    if (!lex.consume(';')) {
        err = QString("expected ';' at byte %1").arg(lex.pos());
        return false;
    }
    for (;;) {
        lex.skipWhitespace();
        if (lex.atEnd() || !lex.peek().isLetter()) break;
        SgfProp prop;
        if (!lex.consumeIdent(prop.id)) break;
        QString value;
        while (lex.consumeValue(value))
            prop.values.append(value);
        node.props.append(prop);
    }
    return true;
}

bool parseNodeList(Lexer& lex, Variation* var, QString& err) {
    // nodes and sub-variations can interleave in arbitrary order
    for (;;) {
        lex.skipWhitespace();
        if (lex.atEnd()) break;
        const QChar c = lex.peek();
        if (c == ';') {
            SgfNode node;
            if (!parseNode(lex, node, err)) return false;
            var->order.append(qMakePair(false, var->nodes.size()));
            var->nodes.append(node);
        } else if (c == '(') {
            Variation* sub = parseGameTree(lex, err);
            if (!sub) return false;
            var->order.append(qMakePair(true, var->children.size()));
            var->children.append(sub);
        } else {
            break;
        }
    }
    return true;
}

Variation* parseGameTree(Lexer& lex, QString& err) {
    if (!lex.consume('(')) {
        err = QString("expected '(' at byte %1").arg(lex.pos());
        return nullptr;
    }
    Variation* var = new Variation();
    if (!parseNodeList(lex, var, err)) {
        delete var;
        return nullptr;
    }
    if (!lex.consume(')')) {
        err = QString("expected ')' at byte %1").arg(lex.pos());
        delete var;
        return nullptr;
    }
    return var;
}

QString propValue(const SgfNode& node, const QString& id) {
    for (const SgfProp& p : node.props)
        if (p.id == id && !p.values.isEmpty())
            return p.values.first();
    return QString();
}

QStringList propValues(const SgfNode& node, const QString& id) {
    for (const SgfProp& p : node.props)
        if (p.id == id)
            return p.values;
    return QStringList();
}

bool hasProp(const SgfNode& node, const QString& id) {
    for (const SgfProp& p : node.props)
        if (p.id == id) return true;
    return false;
}

// expand a point value list, handling compressed point ranges (aa:cc)
void expandPoints(const QStringList& values, int boardSize,
                  QVector<QPair<int,int>>& out) {
    for (const QString& v : values) {
        const QByteArray raw = v.toLatin1();
        if (raw.size() >= 5 && raw[2] == ':') {
            const QPoint a = SgfCoord::fromSgf(raw[0], raw[1]);
            const QPoint b = SgfCoord::fromSgf(raw[3], raw[4]);
            if (a.x() < 0 || b.x() < 0) continue;
            for (int y = qMin(a.y(), b.y()); y <= qMax(a.y(), b.y()); ++y)
                for (int x = qMin(a.x(), b.x()); x <= qMax(a.x(), b.x()); ++x)
                    out.append(qMakePair(x, y));
        } else if (raw.size() == 2) {
            const QPoint p = SgfCoord::fromSgf(raw[0], raw[1], boardSize);
            if (p.x() >= 0)
                out.append(qMakePair(p.x(), p.y()));
        }
        // other sizes (empty = pass for setup stones) ignored
    }
}

// record setup stones on the game (applied at root position)
void recordSetup(Game* game, const SgfNode& node, int boardSize) {
    QVector<QPair<int,int>> ab, aw;
    expandPoints(propValues(node, "AB"), boardSize, ab);
    expandPoints(propValues(node, "AW"), boardSize, aw);
    for (const auto& pt : ab)
        game->addSetupStone(QPoint(pt.first, pt.second), Stone::Black);
    for (const auto& pt : aw)
        game->addSetupStone(QPoint(pt.first, pt.second), Stone::White);
}

Game* buildGame(Variation* root, QString& err) {
    if (root->nodes.isEmpty()) {
        err = QString("no root node");
        return nullptr;
    }
    const SgfNode& rootNode = root->nodes.first();

    bool ok = false;
    int boardSize = propValue(rootNode, "SZ").toInt(&ok);
    if (!ok || boardSize < 2 || boardSize > Board::MaxSize) boardSize = 19;

    RulesConfig cfg;
    double komi = propValue(rootNode, "KM").toDouble(&ok);
    cfg.komi = ok ? komi : 7.5;
    cfg.handicap = propValue(rootNode, "HA").toInt();
    const QString ru = propValue(rootNode, "RU").toUpper();
    cfg.ruleSet = ru.startsWith("JAP") ? RulesConfig::Japanese : RulesConfig::Chinese;

    Game* game = new Game(boardSize, cfg);

    QMap<QString, QString> metadata;
    static const char* metaKeys[] = {"PB", "PW", "DT", "RE", "GN"};
    for (const char* k : metaKeys) {
        const QString v = propValue(rootNode, k);
        if (!v.isEmpty()) metadata.insert(QString::fromLatin1(k), v);
    }
    game->setMetadata(metadata);

    // root comment
    const QString rootComment = propValue(rootNode, "C");
    if (!rootComment.isEmpty())
        game->tree().root()->comment = rootComment;

    // root setup stones (AB/AW/HA handicap stones): recorded on the Game so
    // every board rebuild restores them; apply to the live board now
    recordSetup(game, rootNode, boardSize);
    for (const auto& st : game->setupStones())
        const_cast<Board&>(game->board()).setupStone(st.first.x(), st.first.y(), st.second);
    // keep the parser's board as the game's root position: the parser works
    // on its own Board state while walking, applying moves on a scratch board
    // is unnecessary — we build via Game::play which maintains its own board.
    // Root setup stones must live in the root position: play() from an empty
    // board would be wrong, so record them in the root node comment-free way:
    // store in metadata and apply at each rewind. Simplest: stash the root
    // setup list in the Game metadata for the serializer, and apply to the
    // root board now (Game::board() starts empty; setup applies directly).
    // NOTE: subsequent play() calls operate on Game's board which now has the
    // setup stones — correct for handicap games.

    struct Walker {
        Game* game;
        int boardSize;

        void applyMove(const QString& moveId, Stone color) {
            QPoint pos(-1, -1);
            const QByteArray raw = moveId.toLatin1();
            bool illegal = false;
            if (raw.size() == 2) {
                pos = SgfCoord::fromSgf(raw[0], raw[1], boardSize);
                if (pos.x() < 0) { illegal = true; pos = QPoint(-1, -1); }
            } else if (raw.size() != 0) {
                illegal = true;   // malformed value, treat as pass-with-note
            }
            MoveNode* n = nullptr;
            if (!illegal)
                n = game->play(pos, color);
            if (!n) {
                n = game->tree().addChild(game->currentNode(), color, pos);
                n->comment = illegal ? ("[illegal] " + moveId) : "[illegal]";
                game->advanceTo(n);
                // keep the board consistent with the tree position
                game->rewindTo(n);
            }
        }

        bool applyNode(const SgfNode& node) {
            const QString b = propValue(node, "B");
            const QString w = propValue(node, "W");
            if (!b.isEmpty() || (hasProp(node, "B") && b.isEmpty() && !hasProp(node, "W"))) {
                applyMove(b, Stone::Black);   // empty value = pass
            } else if (!w.isEmpty() || (hasProp(node, "W") && w.isEmpty())) {
                applyMove(w, Stone::White);
            }
            const QString c = propValue(node, "C");
            if (!c.isEmpty() && game->currentNode()->comment.isEmpty())
                game->currentNode()->comment = c;
            return true;
        }

        bool walk(Variation* var) {
            MoveNode* branchPoint = nullptr;
            bool lastWasChild = false;
            for (const auto& item : var->order) {
                if (item.first) {
                    // sub-variation: branch from the node just before it
                    if (!branchPoint) branchPoint = game->currentNode();
                    else game->rewindTo(branchPoint);
                    if (!walk(var->children[item.second])) return false;
                    lastWasChild = true;
                } else {
                    // a node after a subvariation continues the parent chain
                    if (lastWasChild) {
                        game->rewindTo(branchPoint);
                        lastWasChild = false;
                    }
                    if (!applyNode(var->nodes[item.second])) return false;
                }
            }
            if (branchPoint)
                game->rewindTo(branchPoint);
            return true;
        }
    };

    Walker walker{game, boardSize};
    walker.walk(root);
    game->rewindTo(game->tree().root());
    return game;
}

QString escapeSgfValue(const QString& v) {
    QString out;
    for (const QChar c : v) {
        if (c == ']' || c == '\\' || c == '(' || c == ')')
            out.append('\\');
        out.append(c);
    }
    return out;
}

// serialize the move chain starting at `node`. When a node has multiple
// children (a branch point), ALL branches are emitted as nested subtrees
// in child order (main line first) — this preserves branch order on reparse.
// The tree root (no move, no comment) was already emitted by serialize() as
// the metadata node — for it we skip the ";" and just emit its branches.
void serializeChain(const Game& game, MoveNode* node, QString& out, bool isRoot = true) {
    const bool isTreeRoot = isRoot && node->color == Stone::Empty
                            && node->pos == QPoint(-1, -1)
                            && node->comment.isEmpty();
    if (!isTreeRoot) {
        out += ";";
        if (node->color == Stone::Black || node->color == Stone::White) {
            char col = 't', row = 't';
            SgfCoord::toSgf(node->pos, game.boardSize(), col, row);
            out += node->color == Stone::Black ? "B[" : "W[";
            if (node->pos.x() >= 0)
                out += QString(col) + row;
            out += "]";
        }
        if (!node->comment.isEmpty())
            out += "C[" + escapeSgfValue(node->comment) + "]";
    }
    if (node->children.size() == 1) {
        serializeChain(game, node->children[0], out, false);
    } else if (node->children.size() > 1) {
        for (MoveNode* child : node->children) {
            out += "(";
            serializeChain(game, child, out, false);
            out += ")";
        }
    }
}

} // anonymous namespace

Game* SgfParser::parse(const QString& text, QString* error) {
    QString err;
    Lexer lex(text);
    Variation* root = parseGameTree(lex, err);
    if (!root) {
        if (error) *error = "SGF parse error at byte " + err;
        delete root;
        return nullptr;
    }
    Game* game = buildGame(root, err);
    delete root;
    if (!game) {
        if (error) *error = "SGF parse error: " + err;
        return nullptr;
    }
    return game;
}

QString SgfParser::serialize(const Game& game) {
    QString out;
    out += "(";
    // root node: metadata
    out += ";GM[1]FF[4]";
    out += QString("SZ[%1]").arg(game.boardSize());
    const QMap<QString, QString>& md = game.metadata();
    if (md.contains("PB")) out += "PB[" + escapeSgfValue(md.value("PB")) + "]";
    if (md.contains("PW")) out += "PW[" + escapeSgfValue(md.value("PW")) + "]";
    if (md.contains("DT")) out += "DT[" + escapeSgfValue(md.value("DT")) + "]";
    if (md.contains("RE")) out += "RE[" + escapeSgfValue(md.value("RE")) + "]";
    if (md.contains("GN")) out += "GN[" + escapeSgfValue(md.value("GN")) + "]";
    out += QString("KM[%1]").arg(game.rules().komi);
    out += QString("HA[%1]").arg(game.rules().handicap);
    // ruleset (review fix #3: RU must survive roundtrip)
    out += game.rules().ruleSet == RulesConfig::Japanese ? "RU[Japanese]"
                                                         : "RU[Chinese]";
    // setup stones (review fix #2: AB/AW must survive roundtrip)
    QString ab, aw;
    for (const auto& st : game.setupStones()) {
        char col = 't', row = 't';
        SgfCoord::toSgf(st.first, game.boardSize(), col, row);
        QString& ref = st.second == Stone::Black ? ab : aw;
        ref += QString("[%1%2]").arg(col).arg(row);
    }
    if (!ab.isEmpty()) out += "AB" + ab;
    if (!aw.isEmpty()) out += "AW" + aw;
    const MoveNode* root = game.tree().root();
    if (!root->comment.isEmpty())
        out += "C[" + escapeSgfValue(root->comment) + "]";
    // moves depth-first, main line first
    serializeChain(game, game.tree().root(), out);
    out += ")";
    return out;
}
