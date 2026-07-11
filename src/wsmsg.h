#pragma once

#include <string>
#include <nlohmann/json.hpp>

class WsMsg {
public:
    WsMsg() = default;
    WsMsg(std::string type, nlohmann::json data, std::string sender, std::string receiver);

    const std::string& getType() const { return m_type; }
    const nlohmann::json& getData() const { return m_data; }
    const std::string& getSender() const { return m_sender; }
    const std::string& getReceiver() const { return m_receiver; }
    void setType(std::string value) { m_type = std::move(value); }
    void setData(nlohmann::json value) { m_data = std::move(value); }
    void setSender(std::string value) { m_sender = std::move(value); }
    void setReceiver(std::string value) { m_receiver = std::move(value); }

    nlohmann::json toJson() const;
    std::string toJsonString() const;
    static WsMsg fromJsonString(const std::string& jsonString);
    static WsMsg createErrorNotFoundMsg(const std::string& receiver);
    static WsMsg createOfflineMsg(const std::string& receiver);
    static WsMsg createErrorPwdMsg(const std::string& receiver);

private:
    std::string m_type;
    nlohmann::json m_data;
    std::string m_sender;
    std::string m_receiver;
};
