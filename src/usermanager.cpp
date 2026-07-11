#include "usermanager.h"
#include "logger_manager.h"

#include <fstream>
#include <nlohmann/json.hpp>

UserManager& UserManager::instance()
{
    static UserManager instance;
    return instance;
}

void UserManager::initialize(const std::filesystem::path& applicationDir)
{
    const auto directory = applicationDir / "data";
    std::filesystem::create_directories(directory);
    m_filePath = directory / "users.json";
    loadUsersFromFile();
}

bool UserManager::tryGetRcsUserBySN(const std::string& sn, RcsUser& user) const
{
    std::lock_guard<std::mutex> lock(m_mutex);
    const auto it = m_users.find(sn);
    if (it == m_users.end()) return false;
    user = it->second;
    return true;
}

void UserManager::insertRcsUser(const RcsUser& user) { updateRcsUser(user); }

void UserManager::updateRcsUser(const RcsUser& user)
{
    {
        std::lock_guard<std::mutex> lock(m_mutex);
        m_users[user.getSn()] = user;
    }
    saveUsersToFile();
}

void UserManager::deleteRcsUser(const std::string& sn)
{
    {
        std::lock_guard<std::mutex> lock(m_mutex);
        m_users.erase(sn);
    }
    saveUsersToFile();
}

void UserManager::setUserOnline(const std::string& sn)
{
    {
        std::lock_guard<std::mutex> lock(m_mutex);
        const auto it = m_users.find(sn);
        if (it == m_users.end()) return;
        it->second.setStatus(1);
        it->second.setLoginDate(RcsUser::currentDateTime());
    }
    saveUsersToFile();
}

void UserManager::setUserOffline(const std::string& sn)
{
    {
        std::lock_guard<std::mutex> lock(m_mutex);
        const auto it = m_users.find(sn);
        if (it == m_users.end()) return;
        it->second.setStatus(0);
    }
    saveUsersToFile();
}

bool UserManager::isUserOnline(const std::string& sn) const
{
    std::lock_guard<std::mutex> lock(m_mutex);
    const auto it = m_users.find(sn);
    return it != m_users.end() && it->second.getStatus() == 1;
}

void UserManager::saveUsersToFile()
{
    nlohmann::json users = nlohmann::json::array();
    {
        std::lock_guard<std::mutex> lock(m_mutex);
        for (const auto& entry : m_users) users.push_back(entry.second.toJson());
    }
    const auto temporary = m_filePath.string() + ".tmp";
    std::ofstream output(temporary, std::ios::trunc);
    output << users.dump(2);
    output.close();
    std::error_code error;
    std::filesystem::remove(m_filePath, error);
    std::filesystem::rename(temporary, m_filePath, error);
    if (error) LOG_ERROR("Failed to save user data: {}", error.message());
}

void UserManager::loadUsersFromFile()
{
    std::ifstream input(m_filePath);
    if (!input) return;
    try {
        nlohmann::json users;
        input >> users;
        if (!users.is_array()) return;
        std::lock_guard<std::mutex> lock(m_mutex);
        for (const auto& value : users) {
            RcsUser user;
            user.fromJson(value);
            user.setStatus(0);
            m_users[user.getSn()] = user;
        }
        LOG_INFO("Loaded {} users from file", m_users.size());
    } catch (const std::exception& error) {
        LOG_WARN("Invalid user data file: {}", error.what());
    }
}
