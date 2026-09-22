#include <QtTest>
#include <QBuffer>
#include "GtpClient.h"

class TestGtpClient : public QObject {
    Q_OBJECT
private:
    QByteArray m_buf;
    QBuffer m_io;
    QVector<QPair<quint64, bool>> m_results;
    QVector<QString> m_bodies;

    GtpClient* makeClient() {
        m_io.close();
        m_io.setBuffer(&m_buf);
        m_io.open(QIODevice::ReadWrite);
        auto* c = new GtpClient(&m_io);
        QObject::connect(c, &GtpClient::responseReceived, this,
            [&](quint64 id, bool ok, const QString& body) {
                m_results.append(qMakePair(id, ok));
                m_bodies.append(body);
            });
        return c;
    }

private slots:
    void init() { m_buf.clear(); m_results.clear(); m_bodies.clear(); }

    void simpleSuccessResponse() {
        auto* c = makeClient();
        m_buf.append("=1 name GNU Go\n\n");
        c->checkReadable();
        QCOMPARE(m_results.size(), 1);
        QCOMPARE(m_results[0].first, quint64(1));
        QCOMPARE(m_results[0].second, true);
        QCOMPARE(m_bodies[0], QString("name GNU Go"));
        delete c;
    }
    void errorResponse() {
        auto* c = makeClient();
        m_buf.append("?3 illegal move\n\n");
        c->checkReadable();
        QCOMPARE(m_results.size(), 1);
        QCOMPARE(m_results[0].first, quint64(3));
        QCOMPARE(m_results[0].second, false);
        QCOMPARE(m_bodies[0], QString("illegal move"));
        delete c;
    }
    void outOfOrderResponses() {
        auto* c = makeClient();
        m_buf.append("=2 ok\n\n=1 first\n\n");
        c->checkReadable();
        QCOMPARE(m_results.size(), 2);
        QCOMPARE(m_results[0].first, quint64(2));   // arrival order
        QCOMPARE(m_results[1].first, quint64(1));
        delete c;
    }
    void chunkedArrival() {
        auto* c = makeClient();
        m_buf.append("=1 par");   // partial: no terminator yet
        c->checkReadable();
        QCOMPARE(m_results.size(), 0);            // nothing complete
        m_buf.append("tial body\n\n");
        c->checkReadable();
        QCOMPARE(m_results.size(), 1);
        QCOMPARE(m_bodies[0], QString("partial body"));
        delete c;
    }
    void commentLinesSkipped() {
        auto* c = makeClient();
        m_buf.append("# some engine log line\n=7 result\n\n");
        c->checkReadable();
        QCOMPARE(m_results.size(), 1);
        QCOMPARE(m_bodies[0], QString("result"));
        delete c;
    }
    void multilineBody() {
        auto* c = makeClient();
        m_buf.append("=5 A1\nB2\n\n");
        c->checkReadable();
        QCOMPARE(m_results.size(), 1);
        QCOMPARE(m_bodies[0], QString("A1\nB2"));
        delete c;
    }
    void sendCommandWritesLine() {
        auto* c = makeClient();
        m_buf.clear();
        c->sendCommand("play B C4", 12);
        m_io.seek(0);
        QCOMPARE(m_io.readAll(), QByteArray("12 play B C4\n"));
        delete c;
    }
    void escapeArg() {
        QCOMPARE(GtpClient::escape(QString("has space")), QString("has space"));
        QCOMPARE(GtpClient::escape(QString("new\nline")), QString("new\\nline"));
    }
    void responseWithoutId() {
        auto* c = makeClient();
        m_buf.append("= unknown format\n\n");
        c->checkReadable();
        QCOMPARE(m_results.size(), 1);
        QCOMPARE(m_results[0].first, quint64(0));   // untagged -> id 0
        delete c;
    }
    void crlfTerminator() {
        auto* c = makeClient();
        m_buf.append("=9 ok\r\n\r\n");
        c->checkReadable();
        QCOMPARE(m_results.size(), 1);
        QCOMPARE(m_bodies[0], QString("ok"));
        delete c;
    }
};

QTEST_GUILESS_MAIN(TestGtpClient)
#include "tst_gtpclient.moc"
