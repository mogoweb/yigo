#pragma once
#include <QWidget>
#include "EngineConfig.h"

class QComboBox;
class QDoubleSpinBox;
class QLabel;
class QLineEdit;
class QPushButton;

class EnginePanel : public QWidget {
    Q_OBJECT
public:
    explicit EnginePanel(QWidget* parent = nullptr);
    EngineConfig config() const;
    void setConfig(const EngineConfig& cfg);
    void setStatus(const QString& text, bool error = false);
    void setRunning(bool running);

signals:
    void startRequested(const EngineConfig& cfg);
    void stopRequested();

protected:
    void changeEvent(QEvent* e) override;

private slots:
    void onToggleClicked();

private:
    void retranslateUi();

    QComboBox* m_type = nullptr;
    QLineEdit* m_path = nullptr;
    QLineEdit* m_args = nullptr;
    QLabel* m_engineLabel = nullptr;
    QLabel* m_execLabel = nullptr;
    QLabel* m_argsLabel = nullptr;
    QLabel* m_status = nullptr;
    QPushButton* m_toggle = nullptr;
    bool m_running = false;
};
