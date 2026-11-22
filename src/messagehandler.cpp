#include "messagehandler.h"
#include "logger_manager.h"
#include "websocketclient.h"
#include "websocketserver.h"
#include "wsmsg.h"

void MessageHandler::handleMessage(WebSocketClient* client, const std::string& message)
{
    if (message == "@heart") return;
    const auto normalized = normalizeForwardedMessage(message,
                                                      client->getSessionId(),
                                                      client->getRemoteAddress());
    if (!normalized.valid) {
        LOG_WARN("Rejected invalid message from {}: {}", client->getSessionId(), normalized.error);
        client->sendMessage(WsMsg("error", "Invalid message format", "server", client->getSessionId()).toJsonString());
        return;
    }
    handleSignalMessage(client, normalized);
}

void MessageHandler::handleSignalMessage(WebSocketClient* client, const NormalizedForwardMessage& message)
{
    if (!m_server->sendMessageToClient(message.receiver, message.payload)) {
        WsMsg offline = WsMsg::createOfflineMsg(client->getSessionId());
        offline.setRole(message.role);
        client->sendMessage(offline.toJsonString());
    }
}
