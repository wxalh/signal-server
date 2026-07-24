#pragma once

#include "rcsuser.h"
#include <filesystem>
#include <mutex>
#include <string>
#include <unordered_map>

class UserManager {
public:
    static UserManager& instance();
    void initialize(const std::filesystem::path& applicationDir);
    bool tryGetRcsUserBySN(const std::string& sn, RcsUser& user) const;
    void insertRcsUser(const RcsUser& user);
    void updateRcsUser(const RcsUser& user);
    void deleteRcsUser(const std::string& sn);
    void setUserOnline(const std::string& sn);
    void setUserOffline(const std::string& sn);
    bool isUserOnline(const std::string& sn) const;

private:
    UserManager() = default;
    void saveUsersToFile();
    void loadUsersFromFile();

    mutable std::mutex m_mutex;
    std::unordered_map<std::string, RcsUser> m_users;
    std::filesystem::path m_filePath;
};
