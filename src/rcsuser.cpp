#include "rcsuser.h"
#include <QJsonDocument>

RcsUser::RcsUser()
    : m_status(0)
    , m_loginDate(QDateTime::currentDateTime())
{
}

RcsUser::RcsUser(const QString &sn, const QString &hostname, const QString &loginIp)
    : m_sn(sn)
    , m_hostname(hostname) 
    , m_loginIp(loginIp)
    , m_status(1)
    , m_loginDate(QDateTime::currentDateTime())
{
}

QJsonObject RcsUser::toJson() const
{
    QJsonObject json;
    json["sn"] = m_sn;
    json["hostname"] = m_hostname;
    json["loginIp"] = m_loginIp;
    json["loginDate"] = m_loginDate.toString(Qt::ISODate);
    json["status"] = m_status;
    return json;
}

void RcsUser::fromJson(const QJsonObject &json)
{
    m_sn = json["sn"].toString();
    m_hostname = json["hostname"].toString();
    m_loginIp = json["loginIp"].toString();
    m_loginDate = QDateTime::fromString(json["loginDate"].toString(), Qt::ISODate);
    m_status = json["status"].toInt();
}
