#ifndef WEBSOCKETSERVER_H
#define WEBSOCKETSERVER_H

#include <QObject>
#include <QWebSocketServer>
#include <QWebSocket>
#include <QHash>
#include <QHostAddress>
#include <QMutex>
#include <QPointer>
#include <QTimer>
#include "websocketclient.h"
#include "usermanager.h"
#include "messagehandler.h"

class WebSocketServer : public QObject
{
    Q_OBJECT

public:
    explicit WebSocketServer(const QString &name, quint16 port = 8080, QObject *parent = nullptr);
    ~WebSocketServer();

    // Server control
    bool start();
    void stop();
    bool isListening() const;

    // Client management
    int getOnlineCount() const;
    bool sendMessageToClient(const QString &sessionId, const QString &message);

    // Server info
    quint16 getPort() const { return m_port; }
    QString getServerName() const { return m_serverName; }

signals:
    void clientConnected(WebSocketClient *client);
    void clientDisconnected(WebSocketClient *client);
    void messageReceived(WebSocketClient *client, const QString &message);
    void serverStarted();
    void serverStopped();
    void serverError(const QString &errorString);

private slots:
    void onNewConnection();
    void onClientDisconnected();
    void onClientMessageReceived(const QString &message);
    void onCleanupTimer();

private:
    QWebSocketServer *m_server;
    QString m_serverName;
    quint16 m_port;
    QHash<QString, WebSocketClient *> m_clients;
    mutable QMutex m_clientsMutex;

    UserManager *m_userManager;
    MessageHandler *m_messageHandler;
    QTimer *m_cleanupTimer;

    bool parseConnectionParams(QWebSocket *socket, QString &sessionId, QString &hostname, QString &installId);
    void setupClient(WebSocketClient *client, const QString &sessionId, const QString &hostname, const QString &installId);
    void sendDeviceIdConflict(QWebSocket *socket, const QString &oldSessionId, const QString &newSessionId);
    void cleanupDisconnectedClients();
    void removeClient(WebSocketClient *client, bool notifyOffline);
};

#endif // WEBSOCKETSERVER_H
