#pragma once

#include "messagehandler.h"
#include "usermanager.h"
#include "websocketclient.h"
#include "websocket_types.h"

#include <asio/steady_timer.hpp>
#include <asio/signal_set.hpp>
#include <cstdint>
#include <memory>
#include <mutex>
#include <string>
#include <unordered_map>

class WebSocketServer {
public:
    WebSocketServer(std::string name, std::uint16_t port);
    ~WebSocketServer();
    bool start();
    void run();
    void stop();
    bool isListening() const { return m_listening; }
    std::size_t getOnlineCount() const;
    bool sendMessageToClient(const std::string& sessionId, const std::string& message);
    std::uint16_t getPort() const { return m_port; }
    const std::string& getServerName() const { return m_serverName; }

private:
    void onOpen(ConnectionHandle handle);
    void onClose(ConnectionHandle handle);
    void onMessage(ConnectionHandle handle, WebSocketEndpoint::message_ptr message);
    void scheduleCleanup();
    void cleanupDisconnectedClients();
    std::shared_ptr<WebSocketClient> findByHandle(ConnectionHandle handle) const;
    static std::unordered_map<std::string, std::string> parseQuery(const std::string& resource);
    static std::string createSessionId();

    WebSocketEndpoint m_endpoint;
    std::string m_serverName;
    std::uint16_t m_port;
    mutable std::mutex m_clientsMutex;
    std::unordered_map<std::string, std::shared_ptr<WebSocketClient>> m_clients;
    UserManager& m_userManager;
    MessageHandler m_messageHandler;
    std::unique_ptr<asio::steady_timer> m_cleanupTimer;
    std::unique_ptr<asio::signal_set> m_signals;
    std::atomic_bool m_listening{false};
};
