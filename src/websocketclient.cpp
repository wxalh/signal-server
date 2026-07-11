#include "websocketclient.h"
#include "logger_manager.h"

WebSocketClient::WebSocketClient(WebSocketEndpoint& endpoint, ConnectionHandle handle, std::string remoteAddress)
    : m_endpoint(endpoint), m_handle(std::move(handle)), m_remoteAddress(std::move(remoteAddress)) {}

void WebSocketClient::sendMessage(const std::string& message)
{
    if (!isConnected()) return;
    websocketpp::lib::error_code error;
    m_endpoint.send(m_handle, message, websocketpp::frame::opcode::text, error);
    if (error) LOG_WARN("Unable to send to {}: {}", m_sessionId, error.message());
}

void WebSocketClient::sendJsonMessage(const nlohmann::json& json) { sendMessage(json.dump()); }

void WebSocketClient::close()
{
    if (!m_connected.exchange(false)) return;
    websocketpp::lib::error_code error;
    m_endpoint.close(m_handle, websocketpp::close::status::normal, "", error);
}
