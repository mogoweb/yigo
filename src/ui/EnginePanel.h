#pragma once
#include <QWidget>
#include "EngineConfig.h"

class QLineEdit;
class QComboBox;
class QLabel;
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

private:
    QLineEdit* m_path = nullptr;
    QLineEdit* m_args = nullptr;
    QComboBox* m_type = nullptr;
    QLabel* m_status = nullptr;
    QPushButton* m_toggle = nullptr;
    bool m_running = false;
};
