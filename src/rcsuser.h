#ifndef RCSUSER_H
#define RCSUSER_H

#include <QString>
#include <QDateTime>
#include <QJsonObject>

class RcsUser
{
public:
    RcsUser();
    RcsUser(const QString &sn, const QString &hostname, const QString &loginIp);

    // Getters
    QString getSn() const { return m_sn; }
    QString getHostname() const { return m_hostname; }
    QString getLoginIp() const { return m_loginIp; }
    QDateTime getLoginDate() const { return m_loginDate; }
    int getStatus() const { return m_status; }

    // Setters
    void setSn(const QString &sn) { m_sn = sn; }
    void setHostname(const QString &hostname) { m_hostname = hostname; }
    void setLoginIp(const QString &loginIp) { m_loginIp = loginIp; }
    void setLoginDate(const QDateTime &loginDate) { m_loginDate = loginDate; }
    void setStatus(int status) { m_status = status; }

    // Convert to JSON
    QJsonObject toJson() const;
    void fromJson(const QJsonObject &json);

private:
    QString m_sn;
    QString m_hostname;
    QString m_loginIp;
    QDateTime m_loginDate;
    int m_status = 0;  // 0: offline, 1: online
};

#endif // RCSUSER_H
