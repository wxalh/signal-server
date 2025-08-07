#include "websocketserver.h"
#include "logger_manager.h"
#include <QUrlQuery>
#include <QMutexLocker>

WebSocketServer::WebSocketServer(const QString &name, quint16 port, QObject *parent)
    : QObject(parent)
    , m_server(nullptr)
    , m_serverName(name)
    , m_port(port)
    , m_onlineCount(0)
    , m_userManager(&UserManager::instance())
    , m_messageHandler(new MessageHandler(this, this))
    , m_cleanupTimer(new QTimer(this))
{
    // Setup cleanup timer to periodically clean disconnected clients
    m_cleanupTimer->setInterval(30000); // 30 seconds
    connect(m_cleanupTimer, &QTimer::timeout, this, &WebSocketServer::onCleanupTimer);
}

WebSocketServer::~WebSocketServer()
{
    stop();
}

bool WebSocketServer::start()
{
    if (m_server) {
        LOG_WARN("Server is already running");
        return false;
    }
    
    m_server = new QWebSocketServer(m_serverName, QWebSocketServer::NonSecureMode, this);
    
    connect(m_server, &QWebSocketServer::newConnection,
            this, &WebSocketServer::onNewConnection);
    
    if (m_server->listen(QHostAddress::Any, m_port)) {
        LOG_INFO("WebSocket服务器启动成功，监听端口: {}", m_port);
        m_cleanupTimer->start();
        emit serverStarted();
        return true;
    } else {
        QString errorString = m_server->errorString();
        LOG_ERROR("WebSocket服务器启动失败: {}", errorString);
        LOG_ERROR("尝试绑定端口 {} 失败，可能端口已被占用", m_port);
        delete m_server;
        m_server = nullptr;
        emit serverError(errorString);
        return false;
    }
}

void WebSocketServer::stop()
{
    if (!m_server) {
        return;
    }
    
    m_cleanupTimer->stop();
    
    // Close all client connections
    QMutexLocker locker(&m_clientsMutex);
    for (WebSocketClient *client : m_clients) {
        if (client->isConnected()) {
            client->getSocket()->close();
        }
    }
    m_clients.clear();
    locker.unlock();
    
    m_server->close();
    delete m_server;
    m_server = nullptr;
    
    m_onlineCount = 0;
    
    LOG_INFO("WebSocket服务器已停止");
    emit serverStopped();
}

bool WebSocketServer::isListening() const
{
    return m_server && m_server->isListening();
}

WebSocketClient* WebSocketServer::getClient(const QString &sessionId)
{
    QMutexLocker locker(&m_clientsMutex);
    return m_clients.value(sessionId, nullptr);
}

QList<WebSocketClient*> WebSocketServer::getAllClients()
{
    QMutexLocker locker(&m_clientsMutex);
    return m_clients.values();
}

int WebSocketServer::getOnlineCount() const
{
    return m_onlineCount.loadAcquire();
}

void WebSocketServer::removeClient(const QString &sessionId)
{
    QMutexLocker locker(&m_clientsMutex);
    auto it = m_clients.find(sessionId);
    if (it != m_clients.end()) {
        WebSocketClient *client = it.value();
        m_clients.erase(it);
        locker.unlock();
        
        m_onlineCount.fetchAndSubOrdered(1);
        
        // Update user status
        m_userManager->setUserOffline(sessionId);
        
        // Notify other clients
        m_messageHandler->sendOfflineNotificationToAll(client);
        
        emit clientDisconnected(client);
        
        // Delete client later to avoid issues
        client->deleteLater();
    }
}

void WebSocketServer::onNewConnection()
{
    QWebSocket *socket = m_server->nextPendingConnection();
    if (!socket) {
        return;
    }
    
    QString sessionId, hostname;
    if (!parseConnectionParams(socket, sessionId, hostname)) {
        LOG_WARN("Invalid connection parameters, closing connection");
        socket->close();
        socket->deleteLater();
        return;
    }
    
    // Create client wrapper
    WebSocketClient *client = new WebSocketClient(socket, this);
    setupClient(client, sessionId, hostname);
    
    // Add to client map
    {
        QMutexLocker locker(&m_clientsMutex);
        // Remove existing client with same session ID if any
        auto it = m_clients.find(sessionId);
        if (it != m_clients.end()) {
            WebSocketClient *oldClient = it.value();
            m_clients.erase(it);
            oldClient->deleteLater();
        }
        m_clients[sessionId] = client;
    }
    
    int count = m_onlineCount.fetchAndAddOrdered(1) + 1;
    LOG_INFO("连接打开: {}-{}，当前连接数为：{}", hostname, sessionId, count);
    
    // Connect client signals
    connect(client, &WebSocketClient::messageReceived,
            this, &WebSocketServer::onClientMessageReceived);
    connect(client, &WebSocketClient::disconnected,
            this, &WebSocketServer::onClientDisconnected);
    
    emit clientConnected(client);
    
    // Send current online users to new client
    m_messageHandler->sendAllOnlineUsersTo(client);
    
    // Notify all clients about new client
    m_messageHandler->sendOnlineNotificationToAll(client);
}

void WebSocketServer::onClientDisconnected()
{
    WebSocketClient *client = qobject_cast<WebSocketClient*>(sender());
    if (!client) {
        return;
    }
    
    QString sessionId = client->getSessionId();
    LOG_INFO("连接关闭: {}，当前连接数为：{}", sessionId, (getOnlineCount() - 1));
    
    removeClient(sessionId);
}

void WebSocketServer::onClientMessageReceived(const QString &message)
{
    WebSocketClient *client = qobject_cast<WebSocketClient*>(sender());
    if (!client) {
        return;
    }
    
    m_messageHandler->handleMessage(client, message);
    emit messageReceived(client, message);
}

void WebSocketServer::onCleanupTimer()
{
    cleanupDisconnectedClients();
}

bool WebSocketServer::parseConnectionParams(QWebSocket *socket, QString &sessionId, QString &hostname)
{
    QUrl url = socket->requestUrl();
    QUrlQuery query(url);
    
    sessionId = query.queryItemValue("sessionId");
    if (sessionId.isEmpty()) {
        return false;
    }
    
    hostname = query.queryItemValue("hostname");
    // hostname is optional
    
    return true;
}

void WebSocketServer::setupClient(WebSocketClient *client, const QString &sessionId, const QString &hostname)
{
    client->setSessionId(sessionId);
    client->setHostname(hostname);
    
    // Create or update user record
    RcsUser *existingUser = m_userManager->selectRcsUserBySN(sessionId);
    if (existingUser) {
        // Update existing user
        existingUser->setStatus(1);
        existingUser->setHostname(hostname);
        existingUser->setLoginIp(client->getRemoteAddress().toString());
        existingUser->setLoginDate(QDateTime::currentDateTime());
        m_userManager->updateRcsUser(*existingUser);
        client->setRcsUser(*existingUser);
    } else {
        // Create new user
        RcsUser newUser(sessionId, hostname, client->getRemoteAddress().toString());
        m_userManager->insertRcsUser(newUser);
        client->setRcsUser(newUser);
    }
}

void WebSocketServer::cleanupDisconnectedClients()
{
    QMutexLocker locker(&m_clientsMutex);
    QStringList toRemove;
    
    for (auto it = m_clients.begin(); it != m_clients.end(); ++it) {
        if (!it.value()->isConnected()) {
            toRemove.append(it.key());
        }
    }
    
    for (const QString &sessionId : toRemove) {
        WebSocketClient *client = m_clients.take(sessionId);
        if (client) {
            client->deleteLater();
        }
    }
    
    if (!toRemove.isEmpty()) {
        LOG_DEBUG("清理了{}个断开的客户端连接", toRemove.size());
    }
}
