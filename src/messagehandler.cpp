#include "messagehandler.h"
#include "websocketclient.h"
#include "websocketserver.h"
#include "usermanager.h"
#include "logger_manager.h"
#include <QJsonArray>
#include <QJsonObject>

MessageHandler::MessageHandler(WebSocketServer *server, QObject *parent)
    : QObject(parent), m_server(server)
{
}

void MessageHandler::handleMessage(WebSocketClient *client, const QString &message)
{
    LOG_DEBUG("收到客户端: {} 消息：{}", client->getSessionId(), message);

    // Handle heartbeat
    if (message == "@heart")
    {
        handleHeartbeat(client);
        return;
    }

    // Parse JSON message
    WsMsg wsMsg = WsMsg::fromJsonString(message);

    if (!validateMessage(wsMsg))
    {
        sendErrorMessage(client, "Invalid message format");
        return;
    }

    handleSignalMessage(client, wsMsg, message);
}

void MessageHandler::handleHeartbeat(WebSocketClient *client)
{
    // Just acknowledge heartbeat, no response needed
    LOG_DEBUG("Heartbeat from client: {}", client->getSessionId());
}

void MessageHandler::handleSignalMessage(WebSocketClient *client, const WsMsg &wsMsg, const QString &message)
{
    QString receiver = wsMsg.getReceiver();

    if (receiver.isEmpty())
    {
        WsMsg errorMsg = WsMsg::createErrorNotFoundMsg(wsMsg.getSender());
        client->sendMessage(errorMsg.toJsonString());
        return;
    }

    // Check if receiver user exists
    UserManager &userManager = UserManager::instance();
    RcsUser *receiverUser = userManager.selectRcsUserBySN(receiver);

    if (!receiverUser)
    {
        WsMsg errorMsg = WsMsg::createOfflineMsg(wsMsg.getSender());
        client->sendMessage(errorMsg.toJsonString());
        return;
    }

    // Check if receiver is online
    WebSocketClient *receiverClient = m_server->getClient(receiver);
    if (!receiverClient || !receiverClient->isConnected())
    {
        WsMsg errorMsg = WsMsg::createOfflineMsg(wsMsg.getSender());
        client->sendMessage(errorMsg.toJsonString());
        return;
    }

    // Forward the message
    forwardMessage(client, wsMsg, message);
}

void MessageHandler::forwardMessage(WebSocketClient *sender, const WsMsg &wsMsg, const QString &message)
{
    QString receiver = wsMsg.getReceiver();
    WebSocketClient *receiverClient = m_server->getClient(receiver);

    if (receiverClient && receiverClient->isConnected())
    {
        receiverClient->sendMessage(message);
        LOG_DEBUG("从 {} 到 {} 消息转发: {}", sender->getSessionId(), receiver, message);
    }
}

void MessageHandler::sendOnlineNotificationToAll(WebSocketClient *newClient)
{
    if (m_server->getOnlineCount() <= 1)
    {
        return;
    }

    // Notify all other clients about new client online
    QList<WebSocketClient *> allClients = m_server->getAllClients();
    RcsUser newUser = newClient->getRcsUser();

    for (WebSocketClient *client : allClients)
    {
        if (client->getSessionId() != newClient->getSessionId() && client->isConnected())
        {
            WsMsg wsMsg;
            wsMsg.setType("onlineOne");
            wsMsg.setData(newUser.toJson());
            wsMsg.setReceiver(client->getSessionId());
            wsMsg.setSender("server");

            client->sendMessage(wsMsg.toJsonString());
        }
    }
}

void MessageHandler::sendAllOnlineUsersTo(WebSocketClient *client)
{
    UserManager &userManager = UserManager::instance();
    QList<RcsUser> onlineUsers = userManager.getAllOnlineUsers();

    if (onlineUsers.size() <= 1)
    {
        return;
    }

    QJsonArray userArray;
    QString clientSessionId = client->getSessionId();

    for (const RcsUser &user : onlineUsers)
    {
        if (user.getSn() != clientSessionId)
        {
            userArray.append(user.toJson());
        }
    }

    if (userArray.isEmpty())
    {
        return;
    }

    WsMsg wsMsg;
    wsMsg.setType("onlineList");
    wsMsg.setData(userArray);
    wsMsg.setReceiver(clientSessionId);
    wsMsg.setSender("server");

    client->sendMessage(wsMsg.toJsonString());
}

void MessageHandler::sendOfflineNotificationToAll(WebSocketClient *offlineClient)
{
    if (m_server->getOnlineCount() == 0)
    {
        return;
    }

    // Notify all other clients about client going offline
    QList<WebSocketClient *> allClients = m_server->getAllClients();
    RcsUser offlineUser = offlineClient->getRcsUser();

    for (WebSocketClient *client : allClients)
    {
        if (client->getSessionId() != offlineClient->getSessionId() && client->isConnected())
        {
            WsMsg wsMsg;
            wsMsg.setType("offlineOne");
            wsMsg.setData(offlineUser.toJson());
            wsMsg.setReceiver(client->getSessionId());
            wsMsg.setSender("server");

            client->sendMessage(wsMsg.toJsonString());
        }
    }
}

bool MessageHandler::validateMessage(const WsMsg &wsMsg)
{
    // Basic validation - check if required fields are present
    return !wsMsg.getType().isEmpty();
}

void MessageHandler::sendErrorMessage(WebSocketClient *client, const QString &errorMsg, const QString &type)
{
    WsMsg wsMsg;
    wsMsg.setType(type);
    wsMsg.setData(errorMsg);
    wsMsg.setReceiver(client->getSessionId());
    wsMsg.setSender("server");

    client->sendMessage(wsMsg.toJsonString());
}
