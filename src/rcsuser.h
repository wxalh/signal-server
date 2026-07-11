#pragma once

#include <string>
#include <nlohmann/json.hpp>

class RcsUser {
public:
    RcsUser();
    RcsUser(std::string sn, std::string hostname, std::string loginIp);

    const std::string& getSn() const { return m_sn; }
    const std::string& getHostname() const { return m_hostname; }
    const std::string& getInstallId() const { return m_installId; }
    const std::string& getLoginIp() const { return m_loginIp; }
    const std::string& getLoginDate() const { return m_loginDate; }
    int getStatus() const { return m_status; }
    void setSn(std::string value) { m_sn = std::move(value); }
    void setHostname(std::string value) { m_hostname = std::move(value); }
    void setInstallId(std::string value) { m_installId = std::move(value); }
    void setLoginIp(std::string value) { m_loginIp = std::move(value); }
    void setLoginDate(std::string value) { m_loginDate = std::move(value); }
    void setStatus(int value) { m_status = value; }

    nlohmann::json toJson() const;
    void fromJson(const nlohmann::json& json);
    static std::string currentDateTime();

private:
    std::string m_sn;
    std::string m_hostname;
    std::string m_installId;
    std::string m_loginIp;
    std::string m_loginDate;
    int m_status = 0;
};
