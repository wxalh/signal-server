#include "messagehandler.h"
#include "logger_manager.h"
#include "websocketclient.h"
#include "websocketserver.h"

void MessageHandler::handleMessage(WebSocketClient* client, const std::string& message)
{
    LOG_DEBUG("Message from {}: {}", client->getSessionId(), message);
    if (message == "@heart") return;
    const auto parsed = WsMsg::fromJsonString(message);
    if (parsed.getType().empty()) {
        client->sendMessage(WsMsg("error", "Invalid message format", "server", client->getSessionId()).toJsonString());
        return;
    }
    handleSignalMessage(client, parsed, message);
}

void MessageHandler::handleSignalMessage(WebSocketClient* client, const WsMsg& message, const std::string& original)
{
    if (message.getReceiver().empty()) {
        client->sendMessage(WsMsg::createErrorNotFoundMsg(message.getSender()).toJsonString());
    } else if (!m_server->sendMessageToClient(message.getReceiver(), original)) {
        client->sendMessage(WsMsg::createOfflineMsg(message.getSender()).toJsonString());
    }
}
