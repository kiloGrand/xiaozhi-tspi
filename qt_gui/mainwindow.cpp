#include "mainwindow.h"
#include "ui_mainwindow.h"
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QWidget>
#include <QDebug>
#include <QScrollBar>
#include <QPainter>
#include <QStandardItemModel>
#include <QAbstractItemView>

// 聊天消息自定义委托
class ChatDelegate : public QAbstractItemDelegate
{
public:
    ChatDelegate(QObject *parent = nullptr) : QAbstractItemDelegate(parent) {}

    void paint(QPainter *painter, const QStyleOptionViewItem &option,
               const QModelIndex &index) const override
    {
        int type = index.data(ChatModel::TypeRole).toInt();
        QString content = index.data(ChatModel::ContentRole).toString();
        QString time = index.data(ChatModel::TimeRole).toString();

        bool isRobot = (type == static_cast<int>(MessageType::Robot));

        // 气泡颜色：机器人蓝色，用户绿色
        QColor bubbleColor = isRobot ? QColor(52, 152, 219) : QColor(46, 204, 113);
        QColor textColor = Qt::white;

        // 气泡区域
        int margin = 10;
        int maxWidth = 320;

        // 先设置字体再用它计算
        QFont contentFont = painter->font();
        contentFont.setPointSize(14);
        QFontMetrics fm(contentFont);
        QRect textRect = fm.boundingRect(0, 0, maxWidth - 40, 1000,
                                         Qt::TextWordWrap, content);
        int bubbleWidth = qMin(textRect.width() + 40, maxWidth);
        int bubbleHeight = textRect.height() + 30; // 增加padding避免底部被遮

        // 位置：机器人靠左，用户靠右
        int x;
        if (isRobot) {
            x = option.rect.left() + margin;
        } else {
            x = option.rect.right() - bubbleWidth - margin;
        }
        int y = option.rect.top() + 5;

        // 绘制气泡
        QRect bubbleRect(x, y, bubbleWidth, bubbleHeight);
        painter->setBrush(bubbleColor);
        painter->setPen(Qt::NoPen);
        painter->drawRoundedRect(bubbleRect, 15, 15);

        // 绘制文字（使用较大字体）
        painter->setFont(contentFont);
        painter->setPen(textColor);
        painter->drawText(x + 15, y + 15, bubbleWidth - 30, bubbleHeight - 15,
                          Qt::TextWordWrap, content, nullptr);

        // 绘制时间（气泡下方）
        if (!time.isEmpty()) {
            QFont timeFont = painter->font();
            timeFont.setPointSize(9);
            painter->setFont(timeFont);
            painter->setPen(QColor(150, 150, 150));
            if (isRobot) {
                painter->drawText(x, y + bubbleHeight + 2, bubbleWidth, 15, Qt::AlignLeft, time);
            } else {
                painter->drawText(x, y + bubbleHeight + 2, bubbleWidth, 15, Qt::AlignRight, time);
            }
        }
    }

    QSize sizeHint(const QStyleOptionViewItem &option,
                   const QModelIndex &index) const override
    {
        QString content = index.data(ChatModel::ContentRole).toString();
        QFont contentFont = option.font;
        contentFont.setPointSize(14);
        QFontMetrics fm(contentFont);
        QRect textRect = fm.boundingRect(0, 0, 280, 1000, Qt::TextWordWrap, content);
        int height = textRect.height() + 50; // 气泡 + 时间，增加padding
        return QSize(option.rect.width(), qMax(height, 60));
    }
};

MainWindow::MainWindow(QWidget *parent)
    : QMainWindow(parent)
    , ui(new Ui::MainWindow)
    , m_chatModel(new ChatModel(this))
    , m_ipcWorker(new IPCWorker(this))
{
    ui->setupUi(this);

    setupUI();

    // 连接信号槽
    connect(m_ipcWorker, &IPCWorker::dataReceived, this, &MainWindow::onDataReceived);

    // 启动UDP接收
    m_ipcWorker->start();

    // 初始化数据
    m_currentData.state = 0;
    m_currentData.emotion = "idle";
    m_currentData.wifi = "0";
    m_currentData.battery = "0";
    m_robotEmoji->setText(DataParser::emotionToEmoji("idle"));
}

MainWindow::~MainWindow()
{
    m_ipcWorker->stop();
    delete ui;
}

void MainWindow::setupUI()
{
    // 设置固定尺寸和标题
    setFixedSize(SCREEN_WIDTH, SCREEN_HEIGHT);
    setWindowTitle("AI-xiaozhi");
    setCentralWidget(nullptr);

    // 主容器
    QWidget *centralWidget = new QWidget(this);
    setCentralWidget(centralWidget);

    QVBoxLayout *mainLayout = new QVBoxLayout(centralWidget);
    mainLayout->setContentsMargins(0, 0, 0, 0);
    mainLayout->setSpacing(0);

    // ===== 状态栏 =====
    QWidget *statusBar = new QWidget(this);
    statusBar->setFixedHeight(STATUS_BAR_HEIGHT);
    statusBar->setStyleSheet("background-color: #2c3e50;");

    QHBoxLayout *statusLayout = new QHBoxLayout(statusBar);
    statusLayout->setContentsMargins(20, 0, 20, 0);

    m_batteryLabel = new QLabel("电池: 0%", this);
    m_batteryLabel->setStyleSheet("color: white; font-size: 14px;");

    statusLayout->addStretch();
    statusLayout->addWidget(m_batteryLabel);

    mainLayout->addWidget(statusBar);

    // ===== 机器人区域 =====
    QWidget *robotArea = new QWidget(this);
    robotArea->setFixedHeight(ROBOT_AREA_HEIGHT);
    robotArea->setStyleSheet("background-color: #3498db;");

    QVBoxLayout *robotLayout = new QVBoxLayout(robotArea);
    robotLayout->setAlignment(Qt::AlignCenter);
    robotLayout->setSpacing(10);

    m_robotEmoji = new QLabel("😊", this);
    m_robotEmoji->setAlignment(Qt::AlignCenter);
    m_robotEmoji->setStyleSheet("font-size: 100px;");
    m_robotEmoji->setFont(QFont("Noto Color Emoji", 60));

    robotLayout->addWidget(m_robotEmoji);

    mainLayout->addWidget(robotArea);

    // ===== 聊天消息区域 =====
    m_chatView = new QListView(this);
    m_chatView->setModel(m_chatModel);
    m_chatView->setItemDelegate(new ChatDelegate(this));
    m_chatView->setVerticalScrollMode(QListView::ScrollPerItem);
    m_chatView->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    m_chatView->setStyleSheet(R"(
        QListView {
            background-color: #ecf0f1;
            border: none;
            padding: 5px;
        }
    )");

    mainLayout->addWidget(m_chatView, 1);

    // ===== 输入区域 =====
    QWidget *inputArea = new QWidget(this);
    inputArea->setFixedHeight(INPUT_AREA_HEIGHT);
    inputArea->setStyleSheet("background-color: #34495e;");

    QHBoxLayout *inputLayout = new QHBoxLayout(inputArea);
    inputLayout->setContentsMargins(20, 10, 20, 10);

    m_wakeButton = new QPushButton("点击唤醒小智", this);
    m_wakeButton->setStyleSheet(R"(
        QPushButton {
            background-color: #2ecc71;
            color: white;
            border: none;
            border-radius: 10px;
            font-size: 18px;
            padding: 10px;
        }
        QPushButton:pressed {
            background-color: #27ae60;
        }
    )");

    connect(m_wakeButton, &QPushButton::clicked, this, &MainWindow::onWakeButtonClicked);

    inputLayout->addWidget(m_wakeButton);

    mainLayout->addWidget(inputArea);
}

void MainWindow::onDataReceived(RobotData* data)
{
    qDebug() << "MainWindow::onDataReceived - type:" << data->msgType << "state:" << data->state << "text:" << data->text << "emotion:" << data->emotion;

    // 保存数据副本
    QString text = data->text;
    QString msgType = data->msgType;
    QString emotion = data->emotion;
    m_currentData = *data;
    delete data;

    // 更新状态栏
    updateStatusBar();

    // 只有llm类型的消息才更新表情（tts没有emotion字段）
    if (msgType == "llm" && !emotion.isEmpty()) {
        m_robotEmoji->setText(DataParser::emotionToEmoji(emotion));
    }

    // 如果有文本，添加到聊天记录（llm类型的text用于提取表情，不显示在聊天区域）
    if (!text.isEmpty() && msgType != "llm") {
        // 根据msgType判断消息来源: stt=用户, tts=机器人
        MessageType type = (msgType == "stt") ? MessageType::User : MessageType::Robot;
        m_chatModel->addMessage(ChatMessage(type, text));

        // 滚动到底部
        if (m_chatModel->rowCount() > 0) {
            m_chatView->scrollTo(m_chatModel->index(m_chatModel->rowCount() - 1));
        }
    }
}

void MainWindow::onWakeButtonClicked()
{
    qDebug() << "点击: 唤醒聊天机器人";
}

void MainWindow::updateStatusBar()
{
    m_batteryLabel->setText("电池: " + m_currentData.battery + "%");
}
