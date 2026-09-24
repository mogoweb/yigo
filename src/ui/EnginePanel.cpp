#include "EnginePanel.h"
#include <QComboBox>
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
    form->addRow(tr("Engine"), m_type);
    m_path = new QLineEdit(this);
    m_path->setPlaceholderText(tr("/path/to/katago"));
    form->addRow(tr("Executable"), m_path);
    m_args = new QLineEdit(this);
    m_args->setPlaceholderText(tr("-model weights.bin.gz -config analysis.cfg"));
    form->addRow(tr("Arguments"), m_args);
    m_status = new QLabel(tr("Stopped"), this);
    form->addRow(m_status);
    m_toggle = new QPushButton(tr("Start"), this);
    form->addRow(m_toggle);
    connect(m_toggle, &QPushButton::clicked, this, [this] {
        if (m_running) { Q_EMIT stopRequested(); return; }
        if (m_path->text().trimmed().isEmpty()) {
            setStatus(tr("Engine path required"), true);
            return;
        }
        Q_EMIT startRequested(config());
    });
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
