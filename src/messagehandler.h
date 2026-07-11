#pragma once

#include "wsmsg.h"
#include <string>

class WebSocketClient;
class WebSocketServer;

class MessageHandler {
public:
    explicit MessageHandler(WebSocketServer* server) : m_server(server) {}
    void handleMessage(WebSocketClient* client, const std::string& message);

private:
    WebSocketServer* m_server;
    void handleSignalMessage(WebSocketClient* client, const WsMsg& message, const std::string& original);
};
