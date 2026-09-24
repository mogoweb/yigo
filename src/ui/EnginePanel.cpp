#include "EnginePanel.h"
#include <QComboBox>
#include <QEvent>
#include <QFormLayout>
#include <QLabel>
#include <QLineEdit>
#include <QPushButton>
#include <QVBoxLayout>

EnginePanel::EnginePanel(QWidget* parent) : QWidget(parent) {
    auto* form = new QFormLayout(this);
    m_type = new QComboBox(this);
    m_type->addItem("KataGo");          // index 0 -> KataGo
    m_type->addItem("LeelaZero");       // index 1 -> LeelaZero
    m_engineLabel = new QLabel(this);
    form->addRow(m_engineLabel, m_type);
    m_path = new QLineEdit(this);
    m_execLabel = new QLabel(this);
    form->addRow(m_execLabel, m_path);
    m_args = new QLineEdit(this);
    m_argsLabel = new QLabel(this);
    form->addRow(m_argsLabel, m_args);
    m_status = new QLabel(this);
    form->addRow(m_status);
    m_toggle = new QPushButton(this);
    form->addRow(m_toggle);
    m_running = false;
    retranslateUi();
    connect(m_toggle, &QPushButton::clicked, this, &EnginePanel::onToggleClicked);
}

void EnginePanel::retranslateUi() {
    m_engineLabel->setText(tr("Engine"));
    m_execLabel->setText(tr("Executable"));
    m_argsLabel->setText(tr("Arguments"));
    m_path->setPlaceholderText(tr("/path/to/katago"));
    m_args->setPlaceholderText(tr("-model weights.bin.gz -config analysis.cfg"));
    m_status->setText(tr("Stopped"));
    m_toggle->setText(m_running ? tr("Stop") : tr("Start"));
}

void EnginePanel::changeEvent(QEvent* e) {
    // dynamic retranslation: refresh all static texts on language change
    if (e->type() == QEvent::LanguageChange)
        retranslateUi();
    QWidget::changeEvent(e);
}

void EnginePanel::onToggleClicked() {
    if (m_running) { Q_EMIT stopRequested(); return; }
    if (m_path->text().trimmed().isEmpty()) {
        setStatus(tr("Engine path required"), true);
        return;
    }
    Q_EMIT startRequested(config());
}

EngineConfig EnginePanel::config() const {
    EngineConfig cfg;
    cfg.type = m_type->currentIndex() == 0 ? EngineConfig::KataGo
                                           : EngineConfig::LeelaZero;
    cfg.executable = m_path->text().trimmed();
    cfg.baseArgs = m_args->text().split(' ', QString::SkipEmptyParts);
    cfg.gtpCommand = cfg.type == EngineConfig::KataGo
                         ? QStringLiteral("kata-analyze interval 50")
                         : QStringLiteral("lz-analyze");
    return cfg;
}

void EnginePanel::setConfig(const EngineConfig& cfg) {
    m_type->setCurrentIndex(cfg.type == EngineConfig::KataGo ? 0 : 1);
    m_path->setText(cfg.executable);
    m_args->setText(cfg.baseArgs.join(' '));
}

void EnginePanel::setStatus(const QString& text, bool error) {
    m_status->setText(text);
    m_status->setStyleSheet(error ? "color: #d44;" : QString());
}

void EnginePanel::setRunning(bool running) {
    m_running = running;
    m_toggle->setText(running ? tr("Stop") : tr("Start"));
}
