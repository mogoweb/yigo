#pragma once
#include <QDialog>

#include "GameController.h"

class QComboBox;
class QDoubleSpinBox;
class QSpinBox;
class QCheckBox;

class NewGameDialog : public QDialog {
    Q_OBJECT
public:
    explicit NewGameDialog(QWidget* parent = nullptr);
    GameSetup setup() const;
    void setSetup(const GameSetup& s);

private:
    QComboBox* m_size = nullptr;
    QDoubleSpinBox* m_komi = nullptr;
    QSpinBox* m_handicap = nullptr;
    QCheckBox* m_blackAI = nullptr;
    QCheckBox* m_whiteAI = nullptr;
};
