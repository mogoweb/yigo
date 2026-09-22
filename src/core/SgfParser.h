#pragma once
#include <QMap>
#include <QString>

#include "Game.h"

class SgfParser {
public:
    // parse into a new Game (tree + rules); returns nullptr on failure and
    // fills *error with "SGF parse error at byte N: <detail>"
    static Game* parse(const QString& text, QString* error = nullptr);
    // serialize: root metadata + main line + all branches (depth first)
    static QString serialize(const Game& game);
};
