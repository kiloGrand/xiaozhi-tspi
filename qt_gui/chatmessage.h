#ifndef CHATMESSAGE_H
#define CHATMESSAGE_H

#include <QString>
#include <QDateTime>

enum class MessageType {
    User,       // 用户消息
    Robot       // 机器人消息
};

struct ChatMessage {
    MessageType type;      // 消息类型
    QString content;       // 消息内容
    QDateTime timestamp;   // 时间戳

    ChatMessage(MessageType t, const QString& c)
        : type(t), content(c), timestamp(QDateTime::currentDateTime()) {}
};

#endif // CHATMESSAGE_H
