#ifndef MAINWINDOW_H
#define MAINWINDOW_H

#include <QMainWindow>
#include <QListView>
#include <QLabel>
#include <QPushButton>
#include "constants.h"
#include "chatmodel.h"
#include "ipcworker.h"
#include "dataparser.h"

QT_BEGIN_NAMESPACE
namespace Ui { class MainWindow; }
QT_END_NAMESPACE

class MainWindow : public QMainWindow
{
    Q_OBJECT

public:
    MainWindow(QWidget *parent = nullptr);
    ~MainWindow();

private slots:
    void onDataReceived(RobotData* data);
    void onWakeButtonClicked();

private:
    void setupUI();
    void updateStatusBar();

private:
    Ui::MainWindow *ui;

    // UI组件
    QLabel *m_batteryLabel;
    QLabel *m_robotEmoji;
    QListView *m_chatView;
    QPushButton *m_wakeButton;

    // 数据
    ChatModel *m_chatModel;
    IPCWorker *m_ipcWorker;
    RobotData m_currentData;
};

#endif // MAINWINDOW_H
