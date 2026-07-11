#include "websocketserver.h"
#include "logger_manager.h"

#include <algorithm>
#include <chrono>
#include <iomanip>
#include <random>
#include <sstream>
#include <csignal>

WebSocketServer::WebSocketServer(std::string name, std::uint16_t port)
    : m_serverName(std::move(name)), m_port(port), m_userManager(UserManager::instance()), m_messageHandler(this)
{
    m_endpoint.clear_access_channels(websocketpp::log::alevel::all);
    m_endpoint.clear_error_channels(websocketpp::log::elevel::all);
    m_endpoint.init_asio();
    m_endpoint.set_reuse_addr(true);
    m_endpoint.set_open_handler([this](ConnectionHandle handle) { onOpen(handle); });
    m_endpoint.set_close_handler([this](ConnectionHandle handle) { onClose(handle); });
    m_endpoint.set_fail_handler([this](ConnectionHandle handle) { onClose(handle); });
    m_endpoint.set_message_handler([this](ConnectionHandle handle, WebSocketEndpoint::message_ptr message) {
        onMessage(handle, std::move(message));
    });
}

WebSocketServer::~WebSocketServer() { stop(); }

bool WebSocketServer::start()
{
    try {
        m_endpoint.listen(m_port);
        m_endpoint.start_accept();
        m_listening = true;
        m_cleanupTimer = std::make_unique<asio::steady_timer>(m_endpoint.get_io_service());
        m_signals = std::make_unique<asio::signal_set>(m_endpoint.get_io_service(), SIGINT, SIGTERM);
        m_signals->async_wait([this](const std::error_code&, int) { stop(); });
        scheduleCleanup();
        LOG_INFO("WebSocket server listening on port {}", m_port);
        return true;
    } catch (const std::exception& error) {
        LOG_ERROR("Unable to listen on port {}: {}", m_port, error.what());
        return false;
    }
}

void WebSocketServer::run() { m_endpoint.run(); }

void WebSocketServer::stop()
{
    if (!m_listening.exchange(false)) return;
    websocketpp::lib::error_code error;
    m_endpoint.stop_listening(error);
    if (m_cleanupTimer) m_cleanupTimer->cancel();
    std::vector<std::shared_ptr<WebSocketClient>> clients;
    {
        std::lock_guard<std::mutex> lock(m_clientsMutex);
        for (const auto& entry : m_clients) clients.push_back(entry.second);
        m_clients.clear();
    }
    for (const auto& client : clients) {
        m_userManager.setUserOffline(client->getSessionId());
        client->close();
    }
}

std::size_t WebSocketServer::getOnlineCount() const
{
    std::lock_guard<std::mutex> lock(m_clientsMutex);
    return m_clients.size();
}

bool WebSocketServer::sendMessageToClient(const std::string& sessionId, const std::string& message)
{
    std::shared_ptr<WebSocketClient> client;
    {
        std::lock_guard<std::mutex> lock(m_clientsMutex);
        const auto it = m_clients.find(sessionId);
        if (it != m_clients.end()) client = it->second;
    }
    if (!client || !client->isConnected()) return false;
    client->sendMessage(message);
    return true;
}

void WebSocketServer::onOpen(ConnectionHandle handle)
{
    auto connection = m_endpoint.get_con_from_hdl(handle);
    const auto query = parseQuery(connection->get_resource());
    const auto sessionIt = query.find("sessionId");
    if (sessionIt == query.end() || sessionIt->second.empty()) {
        websocketpp::lib::error_code error;
        m_endpoint.close(handle, websocketpp::close::status::policy_violation, "sessionId is required", error);
        return;
    }

    const auto sessionId = sessionIt->second;
    const auto hostnameIt = query.find("hostname");
    const auto installIt = query.find("installId");
    const auto hostname = hostnameIt == query.end() ? "" : hostnameIt->second;
    auto installId = installIt == query.end() ? "" : installIt->second;
    std::transform(installId.begin(), installId.end(), installId.begin(), [](unsigned char c) { return static_cast<char>(std::toupper(c)); });

    std::shared_ptr<WebSocketClient> oldClient;
    std::string existingInstallId;
    {
        std::lock_guard<std::mutex> lock(m_clientsMutex);
        const auto it = m_clients.find(sessionId);
        if (it != m_clients.end()) {
            oldClient = it->second;
            existingInstallId = oldClient->getInstallId();
        }
    }
    RcsUser existingUser;
    const bool knownUser = m_userManager.tryGetRcsUserBySN(sessionId, existingUser);
    if (existingInstallId.empty() && knownUser) existingInstallId = existingUser.getInstallId();
    if (!installId.empty() && !existingInstallId.empty() && installId != existingInstallId) {
        const auto replacement = createSessionId();
        nlohmann::json response = {{"type", "deviceIdConflict"}, {"sender", "server"}, {"receiver", sessionId},
            {"data", {{"reason", "duplicate_uuid"}, {"oldSessionId", sessionId}, {"newSessionId", replacement}}}};
        m_endpoint.send(handle, response.dump(), websocketpp::frame::opcode::text);
        websocketpp::lib::error_code error;
        m_endpoint.close(handle, websocketpp::close::status::policy_violation, "duplicate uuid", error);
        return;
    }

    auto client = std::make_shared<WebSocketClient>(m_endpoint, handle, connection->get_remote_endpoint());
    client->setSessionId(sessionId);
    client->setHostname(hostname);
    client->setInstallId(installId);
    RcsUser user = knownUser ? existingUser : RcsUser(sessionId, hostname, client->getRemoteAddress());
    user.setStatus(1);
    user.setHostname(hostname);
    user.setInstallId(installId);
    user.setLoginIp(client->getRemoteAddress());
    user.setLoginDate(RcsUser::currentDateTime());
    client->setRcsUser(user);
    m_userManager.updateRcsUser(user);
    {
        std::lock_guard<std::mutex> lock(m_clientsMutex);
        m_clients[sessionId] = client;
    }
    if (oldClient) oldClient->close();
    LOG_INFO("Client connected: {} from {} ({}), online={}", sessionId, client->getRemoteAddress(), hostname, getOnlineCount());
}

void WebSocketServer::onClose(ConnectionHandle handle)
{
    const auto client = findByHandle(handle);
    if (!client) return;
    client->setDisconnected();
    {
        std::lock_guard<std::mutex> lock(m_clientsMutex);
        const auto it = m_clients.find(client->getSessionId());
        if (it != m_clients.end() && it->second == client) m_clients.erase(it);
    }
    m_userManager.setUserOffline(client->getSessionId());
    LOG_INFO("Client disconnected: {}, online={}", client->getSessionId(), getOnlineCount());
}

void WebSocketServer::onMessage(ConnectionHandle handle, WebSocketEndpoint::message_ptr message)
{
    const auto client = findByHandle(handle);
    if (client && message->get_opcode() == websocketpp::frame::opcode::text) m_messageHandler.handleMessage(client.get(), message->get_payload());
}

std::shared_ptr<WebSocketClient> WebSocketServer::findByHandle(ConnectionHandle handle) const
{
    std::owner_less<ConnectionHandle> less;
    std::lock_guard<std::mutex> lock(m_clientsMutex);
    for (const auto& entry : m_clients) {
        const auto candidate = entry.second->getHandle();
        if (!less(candidate, handle) && !less(handle, candidate)) return entry.second;
    }
    return {};
}

void WebSocketServer::scheduleCleanup()
{
    m_cleanupTimer->expires_after(std::chrono::seconds(30));
    m_cleanupTimer->async_wait([this](const std::error_code& error) {
        if (!error && m_listening) {
            cleanupDisconnectedClients();
            scheduleCleanup();
        }
    });
}

void WebSocketServer::cleanupDisconnectedClients()
{
    std::lock_guard<std::mutex> lock(m_clientsMutex);
    for (auto it = m_clients.begin(); it != m_clients.end();) {
        if (!it->second->isConnected()) it = m_clients.erase(it); else ++it;
    }
}

std::unordered_map<std::string, std::string> WebSocketServer::parseQuery(const std::string& resource)
{
    const auto decode = [](const std::string& value) {
        std::string decoded;
        decoded.reserve(value.size());
        for (std::size_t i = 0; i < value.size(); ++i) {
            if (value[i] == '+' ) decoded.push_back(' ');
            else if (value[i] == '%' && i + 2 < value.size()) {
                const auto hex = value.substr(i + 1, 2);
                try {
                    decoded.push_back(static_cast<char>(std::stoul(hex, nullptr, 16)));
                    i += 2;
                } catch (...) { decoded.push_back(value[i]); }
            } else decoded.push_back(value[i]);
        }
        return decoded;
    };
    std::unordered_map<std::string, std::string> result;
    const auto question = resource.find('?');
    if (question == std::string::npos) return result;
    std::istringstream input(resource.substr(question + 1));
    std::string item;
    while (std::getline(input, item, '&')) {
        const auto equal = item.find('=');
        result[decode(item.substr(0, equal))] = equal == std::string::npos ? "" : decode(item.substr(equal + 1));
    }
    return result;
}

std::string WebSocketServer::createSessionId()
{
    static thread_local std::mt19937_64 random(std::random_device{}());
    std::ostringstream out;
    out << std::uppercase << std::hex << std::setfill('0');
    for (int i = 0; i < 4; ++i) out << std::setw(8) << static_cast<std::uint32_t>(random());
    return out.str();
}
