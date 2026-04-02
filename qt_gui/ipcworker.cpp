#include "ipcworker.h"
#include <QUdpSocket>
#include <QNetworkDatagram>
#include <QDebug>
#include <QThread>

class IPCWorkerPrivate : public QThread
{
    Q_OBJECT
public:
    explicit IPCWorkerPrivate(QObject *parent = nullptr)
        : QThread(parent), m_socket(nullptr), m_running(true) {}

    ~IPCWorkerPrivate() {
        stop();
        wait();
    }

    void stop() {
        m_running = false;
        if (m_socket) {
            m_socket->close();
        }
    }

protected:
    void run() override {
        m_socket = new QUdpSocket();

        if (!m_socket->bind(QHostAddress::LocalHost, UI_PORT_DOWN)) {
            qWarning() << "Failed to bind UDP port" << UI_PORT_DOWN;
            return;
        }

        qDebug() << "IPC Worker started, listening on port" << UI_PORT_DOWN;

        while (m_running) {
            if (m_socket->hasPendingDatagrams()) {
                QNetworkDatagram datagram = m_socket->receiveDatagram();
                QString data = QString::fromUtf8(datagram.data());
                qDebug() << "=== RAW UDP received ===" << data << "===";

                RobotData parsed = DataParser::parse(data);
                if (parsed.valid) {
                    qDebug() << "IPCWorker: parsed - type:" << parsed.msgType << "state:" << parsed.state << "text:" << parsed.text;
                    emit dataReceived(new RobotData(parsed));
                } else {
                    qWarning() << "IPCWorker: data parsing failed for:" << data;
                }
            }
            msleep(50);
        }

        m_socket->close();
        delete m_socket;
        m_socket = nullptr;
    }

signals:
    void dataReceived(RobotData* data);

private:
    QUdpSocket *m_socket;
    bool m_running;
};

IPCWorker::IPCWorker(QObject *parent)
    : QObject(parent), m_worker(nullptr)
{}

IPCWorker::~IPCWorker()
{
    stop();
}

void IPCWorker::start()
{
    if (!m_worker) {
        m_worker = new IPCWorkerPrivate(this);
        connect(m_worker, &IPCWorkerPrivate::dataReceived,
                this, &IPCWorker::dataReceived);
        m_worker->start();
    }
}

void IPCWorker::stop()
{
    if (m_worker) {
        m_worker->stop();
        m_worker = nullptr;
    }
}

#include "ipcworker.moc"
