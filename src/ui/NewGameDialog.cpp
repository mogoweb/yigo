#include "NewGameDialog.h"
#include <QCheckBox>
#include <QComboBox>
#include <QDialogButtonBox>
#include <QDoubleSpinBox>
#include <QFormLayout>
#include <QSpinBox>
#include <QVBoxLayout>

NewGameDialog::NewGameDialog(QWidget* parent) : QDialog(parent) {
    setWindowTitle(tr("New Game"));
    auto* form = new QFormLayout;
    m_size = new QComboBox(this);
    m_size->addItem("19 x 19", 19);
    m_size->addItem("13 x 13", 13);
    m_size->addItem("9 x 9", 9);
    form->addRow(tr("Board size"), m_size);
    m_komi = new QDoubleSpinBox(this);
    m_komi->setRange(0.0, 30.0);
    m_komi->setSingleStep(0.5);
    m_komi->setValue(7.5);
    form->addRow(tr("Komi"), m_komi);
    m_handicap = new QSpinBox(this);
    m_handicap->setRange(0, 9);
    form->addRow(tr("Handicap stones"), m_handicap);
    m_blackAI = new QCheckBox(tr("Black is played by the engine"), this);
    form->addRow(m_blackAI);
    m_whiteAI = new QCheckBox(tr("White is played by the engine"), this);
    form->addRow(m_whiteAI);
    auto* buttons = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel, this);
    connect(buttons, &QDialogButtonBox::accepted, this, &QDialog::accept);
    connect(buttons, &QDialogButtonBox::rejected, this, &QDialog::reject);
    auto* layout = new QVBoxLayout(this);
    layout->addLayout(form);
    layout->addWidget(buttons);
}

void NewGameDialog::setSetup(const GameSetup& s) {
    const int idx = m_size->findData(s.boardSize);
    if (idx >= 0) m_size->setCurrentIndex(idx);
    m_komi->setValue(s.komi);
    m_handicap->setValue(s.handicap);
    m_blackAI->setChecked(s.black.kind == PlayerConfig::AI);
    m_whiteAI->setChecked(s.white.kind == PlayerConfig::AI);
}

GameSetup NewGameDialog::setup() const {
    GameSetup s;
    s.boardSize = m_size->currentData().toInt();
    s.komi = m_komi->value();
    s.handicap = m_handicap->value();
    s.black.kind = m_blackAI->isChecked() ? PlayerConfig::AI : PlayerConfig::Human;
    s.white.kind = m_whiteAI->isChecked() ? PlayerConfig::AI : PlayerConfig::Human;
    return s;
}
