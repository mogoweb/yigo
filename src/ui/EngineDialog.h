#pragma once
#include <QDialog>

class EnginePanel;

// Non-modal settings dialog hosting the EnginePanel. Single instance owned
// by MainWindow: closing only hides the dialog so the panel keeps its live
// state (engine running, connected name) across open/close cycles.
class EngineDialog : public QDialog {
    Q_OBJECT
public:
    explicit EngineDialog(QWidget* parent = nullptr);
    EnginePanel* panel() const { return m_panel; }

protected:
    void closeEvent(QCloseEvent* e) override;
    void changeEvent(QEvent* e) override;

private:
    void retranslateUi();

    EnginePanel* m_panel = nullptr;
};
