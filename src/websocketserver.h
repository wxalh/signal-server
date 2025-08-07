#ifndef WEBSOCKETSERVER_H
#define WEBSOCKETSERVER_H

#include <QObject>
#include <QWebSocketServer>
#include <QWebSocket>
#include <QHash>
#include <QHostAddress>
#include <QMutex>
#include <QTimer>
#include <QAtomicInt>
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
    WebSocketClient* getClient(const QString &sessionId);
    QList<WebSocketClient*> getAllClients();
    int getOnlineCount() const;
    void removeClient(const QString &sessionId);

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
    QHash<QString, WebSocketClient*> m_clients;
    QMutex m_clientsMutex;
    QAtomicInt m_onlineCount;
    
    UserManager *m_userManager;
    MessageHandler *m_messageHandler;
    QTimer *m_cleanupTimer;

    bool parseConnectionParams(QWebSocket *socket, QString &sessionId, QString &hostname);
    void setupClient(WebSocketClient *client, const QString &sessionId, const QString &hostname);
    void cleanupDisconnectedClients();
};

#endif // WEBSOCKETSERVER_H
