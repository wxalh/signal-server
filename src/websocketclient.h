#ifndef WEBSOCKETCLIENT_H
#define WEBSOCKETCLIENT_H

#include <QObject>
#include <QWebSocket>
#include <QString>
#include <QDateTime>
#include <QHostAddress>
#include "rcsuser.h"

class WebSocketClient : public QObject
{
    Q_OBJECT

public:
    explicit WebSocketClient(QWebSocket *socket, QObject *parent = nullptr);
    ~WebSocketClient();

    // Getters
    QString getSessionId() const { return m_sessionId; }
    QString getHostname() const { return m_hostname; }
    QHostAddress getRemoteAddress() const { return m_remoteAddress; }
    QWebSocket* getSocket() const { return m_socket; }
    RcsUser getRcsUser() const { return m_rcsUser; }
    bool isConnected() const;

    // Setters
    void setSessionId(const QString &sessionId) { m_sessionId = sessionId; }
    void setHostname(const QString &hostname) { m_hostname = hostname; }
    void setRcsUser(const RcsUser &user) { m_rcsUser = user; }

    // Message sending
    void sendMessage(const QString &message);
    void sendJsonMessage(const QJsonObject &json);

signals:
    void messageReceived(const QString &message);
    void disconnected();
    void error(const QString &errorString);

private slots:
    void onTextMessageReceived(const QString &message);
    void onDisconnected();
    void onSocketError();

private:
    QWebSocket *m_socket;
    QString m_sessionId;
    QString m_hostname;
    QHostAddress m_remoteAddress;
    RcsUser m_rcsUser;
};

#endif // WEBSOCKETCLIENT_H
