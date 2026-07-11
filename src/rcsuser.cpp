#include "rcsuser.h"

#include <chrono>
#include <ctime>
#include <iomanip>
#include <sstream>

RcsUser::RcsUser() : m_loginDate(currentDateTime()) {}

RcsUser::RcsUser(std::string sn, std::string hostname, std::string loginIp)
    : m_sn(std::move(sn)), m_hostname(std::move(hostname)), m_loginIp(std::move(loginIp)),
      m_loginDate(currentDateTime()), m_status(1) {}

std::string RcsUser::currentDateTime()
{
    const auto now = std::chrono::system_clock::to_time_t(std::chrono::system_clock::now());
    std::tm tm{};
#ifdef _WIN32
    gmtime_s(&tm, &now);
#else
    gmtime_r(&now, &tm);
#endif
    std::ostringstream out;
    out << std::put_time(&tm, "%Y-%m-%dT%H:%M:%SZ");
    return out.str();
}

nlohmann::json RcsUser::toJson() const
{
    return {{"sn", m_sn}, {"hostname", m_hostname}, {"installId", m_installId},
            {"loginIp", m_loginIp}, {"loginDate", m_loginDate}, {"status", m_status}};
}

void RcsUser::fromJson(const nlohmann::json& json)
{
    m_sn = json.value("sn", "");
    m_hostname = json.value("hostname", "");
    m_installId = json.value("installId", "");
    m_loginIp = json.value("loginIp", "");
    m_loginDate = json.value("loginDate", currentDateTime());
    m_status = json.value("status", 0);
}
