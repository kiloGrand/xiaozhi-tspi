#include "dataparser.h"
#include "constants.h"
#include <QMap>
#include <QJsonDocument>
#include <QJsonObject>
#include <QDebug>

RobotData DataParser::parse(const QString &jsonStr)
{
    RobotData data;
    data.state = 0;
    data.text = "";
    data.emotion = "";
    data.wifi = "0";
    data.battery = "0";
    data.msgType = "";
    data.valid = false;

    qDebug() << "DataParser::parse received:" << jsonStr;

    QJsonDocument doc = QJsonDocument::fromJson(jsonStr.toUtf8());
    if (!doc.isObject()) {
        qWarning() << "DataParser: JSON is not an object";
        return data;
    }

    QJsonObject obj = doc.object();

    // 解析type字段 - 区分用户消息和机器人消息
    if (obj.contains("type") && obj["type"].isString()) {
        data.msgType = obj["type"].toString();
    }

    if (obj.contains("state") && obj["state"].isDouble()) {
        data.state = obj["state"].toInt();
    }
    if (obj.contains("text") && obj["text"].isString()) {
        data.text = obj["text"].toString();
    }
    if (obj.contains("emotion") && obj["emotion"].isString()) {
        data.emotion = obj["emotion"].toString();
    }
    if (obj.contains("wifi")) {
        if (obj["wifi"].isString()) {
            data.wifi = obj["wifi"].toString();
        } else if (obj["wifi"].isDouble()) {
            data.wifi = QString::number(obj["wifi"].toDouble());
        }
    }
    if (obj.contains("battery")) {
        if (obj["battery"].isString()) {
            data.battery = obj["battery"].toString();
        } else if (obj["battery"].isDouble()) {
            data.battery = QString::number(obj["battery"].toDouble());
        }
    }

    data.valid = true;
    return data;
}

QString DataParser::stateToText(int state)
{
    if (state >= 0 && state <= 4) {
        return STATE_TEXT[state];
    }
    return "未知";
}

QString DataParser::emotionToEmoji(const QString &emotion)
{
    static const QMap<QString, QString> emojiMap = {
        {"neutral", "😐"},
        {"happy", "😊"},
        {"laughing", "😆"},
        {"funny", "😂"},
        {"sad", "😢"},
        {"angry", "😠"},
        {"crying", "😭"},
        {"loving", "😍"},
        {"embarrassed", "😳"},
        {"surprised", "😮"},
        {"shocked", "😱"},
        {"thinking", "🤔"},
        {"winking", "😉"},
        {"cool", "😎"},
        {"relaxed", "😌"},
        {"delicious", "😋"},
        {"kissy", "😚"},
        {"confident", "😏"},
        {"sleepy", "😴"},
        {"silly", "🤪"},
        {"confused", "😕"},
        {"idle", "😊"}
    };

    return emojiMap.value(emotion, "😊");
}
