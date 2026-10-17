#include "EngineDialog.h"
#include "EnginePanel.h"

#include <QCloseEvent>
#include <QEvent>
#include <QVBoxLayout>

EngineDialog::EngineDialog(QWidget* parent) : QDialog(parent) {
    m_panel = new EnginePanel(this);
    auto* layout = new QVBoxLayout(this);
    layout->addWidget(m_panel);
    retranslateUi();
}

void EngineDialog::closeEvent(QCloseEvent* e) {
    e->accept();
    hide();   // hide, never delete: MainWindow owns the single instance
}

void EngineDialog::changeEvent(QEvent* e) {
    if (e->type() == QEvent::LanguageChange)
        retranslateUi();
    QDialog::changeEvent(e);
}

void EngineDialog::retranslateUi() {
    setWindowTitle(tr("Engine Settings"));
}
