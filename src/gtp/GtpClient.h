#pragma once
#include <QObject>
#include <QIODevice>
#include <QString>

// GTP line-protocol decoder over an arbitrary QIODevice.
// Response grammar: ("=" | "?") [id] body "\n\n"; "#" lines ignored.
class GtpClient : public QObject {
    Q_OBJECT
public:
    explicit GtpClient(QIODevice* io, QObject* parent = nullptr);
    void sendCommand(const QString& cmd, quint64 id);
    void checkReadable();   // read loop; auto-invoked on readyRead
    void setPaused(bool on) { m_paused = on; }   // stop auto-draining (analysis)
    static QString escape(const QString& arg);

signals:
    void responseReceived(quint64 id, bool success, const QString& body);

private:
    void processBuffer();

    QIODevice* m_io;
    QByteArray m_pending;
    bool m_paused = false;
};
