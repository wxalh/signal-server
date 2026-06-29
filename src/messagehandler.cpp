#include "messagehandler.h"
#include "websocketclient.h"
#include "websocketserver.h"
#include "logger_manager.h"

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

    // Check if receiver is online
    if (!m_server->sendMessageToClient(receiver, message))
    {
        WsMsg errorMsg = WsMsg::createOfflineMsg(wsMsg.getSender());
        client->sendMessage(errorMsg.toJsonString());
        return;
    }

    LOG_DEBUG("从 {} 到 {} 消息转发: {}", client->getSessionId(), receiver, message);
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
