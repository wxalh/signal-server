#pragma once

#include "rcsuser.h"
#include "websocket_types.h"
#include <atomic>
#include <string>

class WebSocketClient {
public:
    WebSocketClient(WebSocketEndpoint& endpoint, ConnectionHandle handle, std::string remoteAddress);

    const std::string& getSessionId() const { return m_sessionId; }
    const std::string& getHostname() const { return m_hostname; }
    const std::string& getInstallId() const { return m_installId; }
    const std::string& getRemoteAddress() const { return m_remoteAddress; }
    ConnectionHandle getHandle() const { return m_handle; }
    const RcsUser& getRcsUser() const { return m_rcsUser; }
    bool isConnected() const { return m_connected.load(); }

    void setSessionId(std::string value) { m_sessionId = std::move(value); }
    void setHostname(std::string value) { m_hostname = std::move(value); }
    void setInstallId(std::string value) { m_installId = std::move(value); }
    void setRcsUser(const RcsUser& value) { m_rcsUser = value; }
    void setDisconnected() { m_connected = false; }
    void sendMessage(const std::string& message);
    void sendJsonMessage(const nlohmann::json& json);
    void close();

private:
    WebSocketEndpoint& m_endpoint;
    ConnectionHandle m_handle;
    std::string m_sessionId;
    std::string m_hostname;
    std::string m_installId;
    std::string m_remoteAddress;
    RcsUser m_rcsUser;
    std::atomic_bool m_connected{true};
};
