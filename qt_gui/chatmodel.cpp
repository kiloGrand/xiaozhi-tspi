#include "chatmodel.h"

ChatModel::ChatModel(QObject *parent)
    : QAbstractListModel(parent)
{}

int ChatModel::rowCount(const QModelIndex &parent) const
{
    Q_UNUSED(parent);
    return m_messages.count();
}

QVariant ChatModel::data(const QModelIndex &index, int role) const
{
    if (!index.isValid() || index.row() >= m_messages.count()) {
        return QVariant();
    }

    const ChatMessage &msg = m_messages.at(index.row());

    switch (role) {
    case TypeRole:
        return static_cast<int>(msg.type);
    case ContentRole:
        return msg.content;
    case TimeRole:
        return msg.timestamp.toString("HH:mm");
    default:
        return QVariant();
    }
}

QHash<int, QByteArray> ChatModel::roleNames() const
{
    QHash<int, QByteArray> roles;
    roles[TypeRole] = "type";
    roles[ContentRole] = "content";
    roles[TimeRole] = "time";
    return roles;
}

void ChatModel::addMessage(const ChatMessage &msg)
{
    beginInsertRows(QModelIndex(), m_messages.count(), m_messages.count());
    m_messages.append(msg);
    endInsertRows();
}

void ChatModel::clear()
{
    beginResetModel();
    m_messages.clear();
    endResetModel();
}
