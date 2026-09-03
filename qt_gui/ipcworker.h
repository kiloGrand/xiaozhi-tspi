#ifndef IPCWORKER_H
#define IPCWORKER_H

#include <QObject>
#include <QString>
#include "constants.h"
#include "dataparser.h"

class QThread;
class IPCWorkerPrivate;

class IPCWorker : public QObject
{
    Q_OBJECT

public:
    explicit IPCWorker(QObject *parent = nullptr);
    ~IPCWorker();

    void start();
    void stop();

signals:
    void dataReceived(RobotData* data);
    void errorOccurred(const QString& msg);
    void stopRequested();

private:
    QThread *m_thread;
    IPCWorkerPrivate *m_worker;
};

#endif // IPCWORKER_H
