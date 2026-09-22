#include "GtpClient.h"

GtpClient::GtpClient(QIODevice* io, QObject* parent)
    : QObject(parent), m_io(io) {
    connect(m_io, &QIODevice::readyRead, this, &GtpClient::checkReadable);
}

void GtpClient::sendCommand(const QString& cmd, quint64 id) {
    const QByteArray line = QByteArray::number(static_cast<qulonglong>(id))
                            + ' ' + cmd.toUtf8() + '\n';
    m_io->write(line);
}

void GtpClient::checkReadable() {
    if (m_paused) return;   // owner is consuming raw stream itself
    m_pending += m_io->readAll();
    processBuffer();
}

void GtpClient::processBuffer() {
    // response ends at "\n\n" (possibly "\r\n\r\n")
    for (;;) {
        int end = m_pending.indexOf("\n\n");
        int endLen = 2;
        const int crlfEnd = m_pending.indexOf("\r\n\r\n");
        if (crlfEnd >= 0 && (end < 0 || crlfEnd < end)) { end = crlfEnd; endLen = 4; }
        if (end < 0) return;   // wait for more data
        const QByteArray chunk = m_pending.left(end);
        m_pending.remove(0, end + endLen);
        // process lines: skip comments and empties, first =/? line is header
        bool success = true;
        quint64 id = 0;
        QString body;
        bool haveHeader = false;
        const QList<QByteArray> lines = chunk.split('\n');
        for (const QByteArray& raw : lines) {
            QByteArray line = raw;
            if (line.endsWith('\r')) line.chop(1);
            if (line.isEmpty()) continue;
            if (line.startsWith('#')) continue;
            if (!haveHeader && (line.startsWith('=') || line.startsWith('?'))) {
                success = line.startsWith('=');
                line.remove(0, 1);
                // optional numeric id
                const int sp = line.indexOf(' ');
                const QByteArray head = (sp < 0) ? line : line.left(sp);
                bool idOk = false;
                const quint64 parsed = head.toULongLong(&idOk);
                if (idOk) {
                    id = parsed;
                    if (sp >= 0) body = QString::fromUtf8(line.mid(sp + 1));
                } else {
                    body = QString::fromUtf8(line);
                }
                haveHeader = true;
                continue;
            }
            if (haveHeader)
                body += '\n' + QString::fromUtf8(line);
        }
        if (haveHeader)
            Q_EMIT responseReceived(id, success, body);
    }
}

QString GtpClient::escape(const QString& arg) {
    QString out = arg;
    out.replace('\n', QStringLiteral("\\n"));
    out.replace(']', QStringLiteral("\\]"));
    return out;
}
