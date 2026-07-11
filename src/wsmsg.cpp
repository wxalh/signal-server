#include "wsmsg.h"

WsMsg::WsMsg(std::string type, nlohmann::json data, std::string sender, std::string receiver)
    : m_type(std::move(type)), m_data(std::move(data)), m_sender(std::move(sender)), m_receiver(std::move(receiver)) {}

nlohmann::json WsMsg::toJson() const
{
    return {{"type", m_type}, {"data", m_data}, {"sender", m_sender}, {"receiver", m_receiver}};
}

std::string WsMsg::toJsonString() const { return toJson().dump(); }

WsMsg WsMsg::fromJsonString(const std::string& jsonString)
{
    WsMsg msg;
    const auto json = nlohmann::json::parse(jsonString, nullptr, false);
    if (!json.is_object()) return msg;
    msg.m_type = json.value("type", "");
    msg.m_sender = json.value("sender", "");
    msg.m_receiver = json.value("receiver", "");
    if (json.contains("data")) msg.m_data = json["data"];
    return msg;
}

WsMsg WsMsg::createErrorNotFoundMsg(const std::string& receiver) { return {"error", "not found recv id", "server", receiver}; }
WsMsg WsMsg::createOfflineMsg(const std::string& receiver) { return {"error", "The controlled end may not be online", "server", receiver}; }
WsMsg WsMsg::createErrorPwdMsg(const std::string& receiver) { return {"error", "The controlled end may not be online", "server", receiver}; }
