#include "websocketserver.h"
#include "logger_manager.h"
#include <QUrlQuery>
#include <QMutexLocker>
#include <QJsonDocument>
#include <QUuid>

WebSocketServer::WebSocketServer(const QString &name, quint16 port, QObject *parent)
    : QObject(parent)
    , m_server(nullptr)
    , m_serverName(name)
    , m_port(port)
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
    
    // Close all client connections and release wrappers
    QList<WebSocketClient*> clients;
    QStringList sessions;
    {
        QMutexLocker locker(&m_clientsMutex);
        clients = m_clients.values();
        sessions = m_clients.keys();
        m_clients.clear();
    }

    for (const QString &sessionId : sessions) {
        m_userManager->setUserOffline(sessionId);
    }

    for (WebSocketClient *client : clients) {
        if (!client) {
            continue;
        }
        disconnect(client, nullptr, this, nullptr);
        if (client->isConnected()) {
            client->getSocket()->close();
        }
        client->deleteLater();
    }
    
    m_server->close();
    delete m_server;
    m_server = nullptr;
    
    LOG_INFO("WebSocket服务器已停止");
    emit serverStopped();
}

bool WebSocketServer::isListening() const
{
    return m_server && m_server->isListening();
}

int WebSocketServer::getOnlineCount() const
{
    QMutexLocker locker(&m_clientsMutex);
    return m_clients.size();
}

bool WebSocketServer::sendMessageToClient(const QString &sessionId, const QString &message)
{
    QPointer<WebSocketClient> client;
    {
        QMutexLocker locker(&m_clientsMutex);
        client = m_clients.value(sessionId, nullptr);
    }

    if (!client || !client->isConnected()) {
        return false;
    }

    client->sendMessage(message);
    return true;
}

void WebSocketServer::onNewConnection()
{
    QWebSocket *socket = m_server->nextPendingConnection();
    if (!socket) {
        return;
    }
    
    QString sessionId, hostname, installId;
    if (!parseConnectionParams(socket, sessionId, hostname, installId)) {
        LOG_WARN("Invalid connection parameters, closing connection");
        socket->close();
        socket->deleteLater();
        return;
    }

    auto createUnusedSessionId = [this]() {
        QString sessionId;
        RcsUser existingUser;
        do {
            sessionId = QUuid::createUuid().toString().remove("{").remove("}").toUpper();
        } while (m_userManager->tryGetRcsUserBySN(sessionId, existingUser));
        return sessionId;
    };

    WebSocketClient *oldClient = nullptr;
    bool duplicateInstall = false;
    QString existingInstallId;
    {
        QMutexLocker locker(&m_clientsMutex);
        oldClient = m_clients.value(sessionId, nullptr);
        if (oldClient && !installId.isEmpty() && !oldClient->getInstallId().isEmpty() && oldClient->getInstallId() != installId) {
            duplicateInstall = true;
            existingInstallId = oldClient->getInstallId();
        }
    }

    if (!duplicateInstall && !installId.isEmpty()) {
        RcsUser existingUser;
        if (m_userManager->tryGetRcsUserBySN(sessionId, existingUser) &&
            !existingUser.getInstallId().isEmpty() &&
            existingUser.getInstallId() != installId) {
            duplicateInstall = true;
            existingInstallId = existingUser.getInstallId();
        }
    }

    if (duplicateInstall) {
        const QString newSessionId = createUnusedSessionId();
        LOG_WARN("Device id conflict: sessionId={} existingInstallId={} newInstallId={}, assigning {}",
                 sessionId, existingInstallId, installId, newSessionId);
        sendDeviceIdConflict(socket, sessionId, newSessionId);
        QTimer::singleShot(200, socket, [socket]() {
            socket->close();
            socket->deleteLater();
        });
        return;
    }
    
    // Create client wrapper
    WebSocketClient *client = new WebSocketClient(socket, this);
    setupClient(client, sessionId, hostname, installId);
    
    oldClient = nullptr;

    // Add to client map
    {
        QMutexLocker locker(&m_clientsMutex);
        // Remove existing client with same session ID if any
        auto it = m_clients.find(sessionId);
        if (it != m_clients.end()) {
            oldClient = it.value();
            m_clients.erase(it);
        }
        m_clients[sessionId] = client;
    }

    if (oldClient) {
        disconnect(oldClient, nullptr, this, nullptr);
        if (oldClient->isConnected()) {
            oldClient->getSocket()->close();
        }
        oldClient->deleteLater();
    }
    
    int count = getOnlineCount();
    LOG_INFO("连接打开: {}-{}，当前连接数为：{}", hostname, sessionId, count);
    
    // Connect client signals
    connect(client, &WebSocketClient::messageReceived,
            this, &WebSocketServer::onClientMessageReceived);
    connect(client, &WebSocketClient::disconnected,
            this, &WebSocketServer::onClientDisconnected);
    
    emit clientConnected(client);
}

void WebSocketServer::onClientDisconnected()
{
    WebSocketClient *client = qobject_cast<WebSocketClient*>(sender());
    if (!client) {
        return;
    }
    
    QString sessionId = client->getSessionId();
    int nextCount = getOnlineCount();
    if (nextCount > 0) {
        --nextCount;
    }
    LOG_INFO("连接关闭: {}，当前连接数为：{}", sessionId, nextCount);

    removeClient(client, true);
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

bool WebSocketServer::parseConnectionParams(QWebSocket *socket, QString &sessionId, QString &hostname, QString &installId)
{
    QUrl url = socket->requestUrl();
    QUrlQuery query(url);
    
    sessionId = query.queryItemValue("sessionId");
    if (sessionId.isEmpty()) {
        return false;
    }
    
    hostname = query.queryItemValue("hostname");
    installId = query.queryItemValue("installId").toUpper();
    // hostname is optional
    
    return true;
}

void WebSocketServer::setupClient(WebSocketClient *client, const QString &sessionId, const QString &hostname, const QString &installId)
{
    client->setSessionId(sessionId);
    client->setHostname(hostname);
    client->setInstallId(installId);
    
    // Create or update user record
    RcsUser existingUser;
    if (m_userManager->tryGetRcsUserBySN(sessionId, existingUser)) {
        // Update existing user
        existingUser.setStatus(1);
        existingUser.setHostname(hostname);
        existingUser.setInstallId(installId);
        existingUser.setLoginIp(client->getRemoteAddress().toString());
        existingUser.setLoginDate(QDateTime::currentDateTime());
        m_userManager->updateRcsUser(existingUser);
        client->setRcsUser(existingUser);
    } else {
        // Create new user
        RcsUser newUser(sessionId, hostname, client->getRemoteAddress().toString());
        newUser.setInstallId(installId);
        m_userManager->insertRcsUser(newUser);
        client->setRcsUser(newUser);
    }
}

void WebSocketServer::sendDeviceIdConflict(QWebSocket *socket, const QString &oldSessionId, const QString &newSessionId)
{
    if (!socket) {
        return;
    }

    QJsonObject data;
    data["reason"] = "duplicate_uuid";
    data["oldSessionId"] = oldSessionId;
    data["newSessionId"] = newSessionId;

    QJsonObject message;
    message["type"] = "deviceIdConflict";
    message["sender"] = "server";
    message["receiver"] = oldSessionId;
    message["data"] = data;

    socket->sendTextMessage(QString::fromUtf8(QJsonDocument(message).toJson(QJsonDocument::Compact)));
    socket->flush();
}

void WebSocketServer::cleanupDisconnectedClients()
{
    QList<WebSocketClient*> toRemove;

    {
        QMutexLocker locker(&m_clientsMutex);
        for (auto it = m_clients.begin(); it != m_clients.end(); ++it) {
            if (!it.value()->isConnected()) {
                toRemove.append(it.value());
            }
        }
    }
    
    for (WebSocketClient *client : toRemove) {
        removeClient(client, true);
    }
    
    if (!toRemove.isEmpty()) {
        LOG_DEBUG("清理了{}个断开的客户端连接", toRemove.size());
    }
}

void WebSocketServer::removeClient(WebSocketClient *client, bool notifyOffline)
{
    if (!client) {
        return;
    }

    const QString sessionId = client->getSessionId();
    bool removed = false;

    {
        QMutexLocker locker(&m_clientsMutex);
        auto it = m_clients.find(sessionId);
        if (it != m_clients.end() && it.value() == client) {
            m_clients.erase(it);
            removed = true;
        }
    }

    if (!removed) {
        disconnect(client, nullptr, this, nullptr);
        if (client->isConnected()) {
            client->getSocket()->close();
        }
        client->deleteLater();
        return;
    }

    if (notifyOffline) {
        m_userManager->setUserOffline(sessionId);
    }

    emit clientDisconnected(client);

    disconnect(client, nullptr, this, nullptr);
    if (client->isConnected()) {
        client->getSocket()->close();
    }
    client->deleteLater();
}
