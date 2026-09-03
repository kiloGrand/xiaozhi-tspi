#include "ipcworker.h"
#include <QUdpSocket>
#include <QNetworkDatagram>
#include <QThread>
#include <QDebug>

class IPCWorkerPrivate : public QObject
{
    Q_OBJECT
public:
    explicit IPCWorkerPrivate(QObject *parent = nullptr)
        : QObject(parent), m_socket(nullptr) {}

public slots:
    void start() {
        m_socket = new QUdpSocket(this);
        connect(m_socket, &QUdpSocket::readyRead,
                this, &IPCWorkerPrivate::onReadyRead);

        if (!m_socket->bind(QHostAddress::LocalHost, UI_PORT_DOWN)) {
            qWarning() << "Failed to bind UDP port" << UI_PORT_DOWN;
            emit errorOccurred("Failed to bind UDP port " + QString::number(UI_PORT_DOWN));
            return;
        }

        qDebug() << "IPC Worker started, listening on port" << UI_PORT_DOWN;
    }

    void stop() {
        if (m_socket) {
            m_socket->close();
        }
    }

signals:
    void dataReceived(RobotData* data);
    void errorOccurred(const QString& msg);

private slots:
    void onReadyRead() {
        while (m_socket->hasPendingDatagrams()) {
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
    }

private:
    QUdpSocket *m_socket;
};

IPCWorker::IPCWorker(QObject *parent)
    : QObject(parent), m_thread(nullptr), m_worker(nullptr)
{}

IPCWorker::~IPCWorker()
{
    stop();
}

void IPCWorker::start()
{
    if (m_thread) return;

    m_thread = new QThread(this);
    m_worker = new IPCWorkerPrivate();

    m_worker->moveToThread(m_thread);

    connect(m_thread, &QThread::started,
            m_worker, &IPCWorkerPrivate::start);
    connect(m_worker, &IPCWorkerPrivate::dataReceived,
            this, &IPCWorker::dataReceived);
    connect(m_worker, &IPCWorkerPrivate::errorOccurred,
            this, &IPCWorker::errorOccurred);
    connect(m_thread, &QThread::finished,
            m_worker, &QObject::deleteLater);
    connect(this, &IPCWorker::stopRequested,
            m_worker, &IPCWorkerPrivate::stop);

    m_thread->start();
}

void IPCWorker::stop()
{
    if (!m_thread) return;

    emit stopRequested();
    m_thread->quit();
    m_thread->wait();

    m_worker = nullptr;
    m_thread = nullptr;
}

#include "ipcworker.moc"
