#ifndef WSMSG_H
#define WSMSG_H

#include <QString>
#include <QJsonObject>
#include <QJsonDocument>
#include <QVariant>

class WsMsg
{
public:
    WsMsg();
    WsMsg(const QString &type, const QVariant &data, const QString &sender, const QString &receiver);

    // Getters
    QString getType() const { return m_type; }
    QVariant getData() const { return m_data; }
    QString getSender() const { return m_sender; }
    QString getReceiver() const { return m_receiver; }

    // Setters
    void setType(const QString &type) { m_type = type; }
    void setData(const QVariant &data) { m_data = data; }
    void setSender(const QString &sender) { m_sender = sender; }
    void setReceiver(const QString &receiver) { m_receiver = receiver; }

    // JSON conversion
    QJsonObject toJson() const;
    void fromJson(const QJsonObject &json);
    QString toJsonString() const;
    static WsMsg fromJsonString(const QString &jsonString);

    // Static error messages
    static WsMsg createErrorNotFoundMsg(const QString &receiver);
    static WsMsg createOfflineMsg(const QString &receiver);
    static WsMsg createErrorPwdMsg(const QString &receiver);

private:
    QString m_type;
    QVariant m_data;
    QString m_sender;
    QString m_receiver;
};

#endif // WSMSG_H
