#include "wsmsg.h"
#include <QJsonObject>
#include <QJsonDocument>
#include <QJsonArray>

WsMsg::WsMsg()
{
}

WsMsg::WsMsg(const QString &type, const QVariant &data, const QString &sender, const QString &receiver)
    : m_type(type)
    , m_data(data)
    , m_sender(sender)
    , m_receiver(receiver)
{
}

QJsonObject WsMsg::toJson() const
{
    QJsonObject json;
    json["type"] = m_type;
    json["sender"] = m_sender;
    json["receiver"] = m_receiver;
    
    // Handle different data types
    if (m_data.canConvert<QJsonObject>()) {
        json["data"] = m_data.toJsonObject();
    } else if (m_data.canConvert<QJsonArray>()) {
        json["data"] = m_data.toJsonArray();
    } else if (m_data.canConvert<QString>()) {
        json["data"] = m_data.toString();
    } else {
        json["data"] = QJsonValue::fromVariant(m_data);
    }
    
    return json;
}

void WsMsg::fromJson(const QJsonObject &json)
{
    m_type = json["type"].toString();
    m_sender = json["sender"].toString();
    m_receiver = json["receiver"].toString();
    
    const QJsonValue dataValue = json["data"];
    if (dataValue.isObject()) {
        m_data = dataValue.toObject();
    } else if (dataValue.isArray()) {
        m_data = dataValue.toArray();
    } else {
        m_data = dataValue.toVariant();
    }
}

QString WsMsg::toJsonString() const
{
    QJsonDocument doc(toJson());
    return doc.toJson(QJsonDocument::Compact);
}

WsMsg WsMsg::fromJsonString(const QString &jsonString)
{
    WsMsg msg;
    QJsonParseError error;
    QJsonDocument doc = QJsonDocument::fromJson(jsonString.toUtf8(), &error);
    
    if (error.error == QJsonParseError::NoError && doc.isObject()) {
        msg.fromJson(doc.object());
    }
    
    return msg;
}

WsMsg WsMsg::createErrorNotFoundMsg(const QString &receiver)
{
    return WsMsg("error", "not found recv id", "server", receiver);
}

WsMsg WsMsg::createOfflineMsg(const QString &receiver)
{
    return WsMsg("error", "The controlled end may not be online", "server", receiver);
}

WsMsg WsMsg::createErrorPwdMsg(const QString &receiver)
{
    return WsMsg("error", "The controlled end may not be online", "server", receiver);
}
