#ifndef DATAPARSER_H
#define DATAPARSER_H

#include <QString>
#include "chatmessage.h"

struct RobotData {
    int state;
    bool hasState;  // 是否包含state字段
    QString text;
    QString emotion;
    QString wifi;
    QString battery;
    QString msgType;  // "stt"(用户) / "tts"或"llm"(机器人) / "state"(状态)
    bool valid;
};

class DataParser
{
public:
    static RobotData parse(const QString& jsonStr);

    // 状态转中文
    static QString stateToText(int state);

    // 情绪转emoji
    static QString emotionToEmoji(const QString& emotion);
};

#endif // DATAPARSER_H
