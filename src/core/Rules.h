#pragma once
#include <QPair>
#include "Board.h"

struct RulesConfig {
    enum RuleSet { Chinese, Japanese } ruleSet = Chinese;
    double komi = 7.5;
    int handicap = 0;
    bool superko = false;
};

struct ScoreResult {
    double blackScore = 0;
    double whiteScore = 0;
    double blackMargin = 0;   // blackScore - whiteScore - komi (positive = black wins)
};

class Rules {
public:
    explicit Rules(const RulesConfig& cfg) : m_cfg(cfg) {}
    ScoreResult score(const Board& board) const;
    ScoreResult score(const Board& board, int blackCaptures, int whiteCaptures) const;
    // territory: only empty points whose border touches a single color
    // (flood-fill), returns (black territory, white territory)
    QPair<int, int> territory(const Board& board) const;

private:
    RulesConfig m_cfg;
};
