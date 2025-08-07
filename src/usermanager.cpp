#include "usermanager.h"
#include "logger_manager.h"
#include <QStandardPaths>
#include <QDir>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonArray>
#include <QFile>
#include <QMutexLocker>

UserManager* UserManager::s_instance = nullptr;

UserManager::UserManager(QObject *parent)
    : QObject(parent)
{
    loadUsersFromFile();
}

UserManager::~UserManager()
{
    // Save all users before destruction
    QMutexLocker locker(&m_mutex);
    for (const RcsUser &user : m_users) {
        saveUserToFile(user);
    }
}

UserManager& UserManager::instance()
{
    if (!s_instance) {
        s_instance = new UserManager();
    }
    return *s_instance;
}

RcsUser* UserManager::selectRcsUserBySN(const QString &sn)
{
    QMutexLocker locker(&m_mutex);
    auto it = m_users.find(sn);
    return (it != m_users.end()) ? &it.value() : nullptr;
}

void UserManager::insertRcsUser(const RcsUser &user)
{
    {
        QMutexLocker locker(&m_mutex);
        m_users[user.getSn()] = user;
    }
    saveUserToFile(user);
    
    if (user.getStatus() == 1) {
        emit userOnline(user);
    }
}

void UserManager::updateRcsUser(const RcsUser &user)
{
    bool wasOnline = false;
    bool isOnline = user.getStatus() == 1;
    
    {
        QMutexLocker locker(&m_mutex);
        auto it = m_users.find(user.getSn());
        if (it != m_users.end()) {
            wasOnline = it.value().getStatus() == 1;
        }
        m_users[user.getSn()] = user;
    }
    
    saveUserToFile(user);
    emit userUpdated(user);
    
    // Emit status change signals
    if (!wasOnline && isOnline) {
        emit userOnline(user);
    } else if (wasOnline && !isOnline) {
        emit userOffline(user);
    }
}

void UserManager::deleteRcsUser(const QString &sn)
{
    RcsUser user;
    {
        QMutexLocker locker(&m_mutex);
        auto it = m_users.find(sn);
        if (it != m_users.end()) {
            user = it.value();
            m_users.erase(it);
        }
    }
    
    if (user.getStatus() == 1) {
        emit userOffline(user);
    }
}

QList<RcsUser> UserManager::selectRcsUserList(const RcsUser &criteria)
{
    QMutexLocker locker(&m_mutex);
    QList<RcsUser> result;
    
    for (const RcsUser &user : m_users) {
        bool matches = true;
        
        // Check status criteria
        if (criteria.getStatus() != 0 && user.getStatus() != criteria.getStatus()) {
            matches = false;
        }
        
        // Add other criteria checks as needed
        
        if (matches) {
            result.append(user);
        }
    }
    
    return result;
}

QList<RcsUser> UserManager::getAllOnlineUsers()
{
    RcsUser criteria;
    criteria.setStatus(1);
    return selectRcsUserList(criteria);
}

int UserManager::getOnlineUserCount()
{
    return getAllOnlineUsers().size();
}

void UserManager::setUserOnline(const QString &sn)
{
    QMutexLocker locker(&m_mutex);
    auto it = m_users.find(sn);
    if (it != m_users.end()) {
        it.value().setStatus(1);
        it.value().setLoginDate(QDateTime::currentDateTime());
        RcsUser user = it.value();
        locker.unlock();
        
        saveUserToFile(user);
        emit userOnline(user);
    }
}

void UserManager::setUserOffline(const QString &sn)
{
    QMutexLocker locker(&m_mutex);
    auto it = m_users.find(sn);
    if (it != m_users.end()) {
        it.value().setStatus(0);
        RcsUser user = it.value();
        locker.unlock();
        
        saveUserToFile(user);
        emit userOffline(user);
    }
}

bool UserManager::isUserOnline(const QString &sn)
{
    QMutexLocker locker(&m_mutex);
    auto it = m_users.find(sn);
    return (it != m_users.end()) && (it.value().getStatus() == 1);
}

QString UserManager::getUserDataFilePath() const
{
    QString dataDir = QStandardPaths::writableLocation(QStandardPaths::AppDataLocation);
    QDir dir(dataDir);
    if (!dir.exists()) {
        dir.mkpath(".");
    }
    return dir.filePath("users.json");
}

void UserManager::saveUserToFile(const RcsUser &user)
{
    QString filePath = getUserDataFilePath();
    
    // Load existing data
    QJsonArray usersArray;
    QFile file(filePath);
    if (file.open(QIODevice::ReadOnly)) {
        QJsonDocument doc = QJsonDocument::fromJson(file.readAll());
        if (doc.isArray()) {
            usersArray = doc.array();
        }
        file.close();
    }
    
    // Update or add user
    bool found = false;
    for (int i = 0; i < usersArray.size(); ++i) {
        QJsonObject userObj = usersArray[i].toObject();
        if (userObj["sn"].toString() == user.getSn()) {
            usersArray[i] = user.toJson();
            found = true;
            break;
        }
    }
    
    if (!found) {
        usersArray.append(user.toJson());
    }
    
    // Save to file
    if (file.open(QIODevice::WriteOnly)) {
        QJsonDocument doc(usersArray);
        file.write(doc.toJson());
        file.close();
    }
}

void UserManager::loadUsersFromFile()
{
    QString filePath = getUserDataFilePath();
    QFile file(filePath);
    
    if (!file.open(QIODevice::ReadOnly)) {
        LOG_DEBUG("No existing user data file found, starting with empty user list");
        return;
    }
    
    QJsonDocument doc = QJsonDocument::fromJson(file.readAll());
    file.close();
    
    if (!doc.isArray()) {
        LOG_WARN("Invalid user data file format");
        return;
    }
    
    QJsonArray usersArray = doc.array();
    QMutexLocker locker(&m_mutex);
    
    for (const QJsonValue &value : usersArray) {
        if (value.isObject()) {
            RcsUser user;
            user.fromJson(value.toObject());
            // Set all users offline on startup
            user.setStatus(0);
            m_users[user.getSn()] = user;
        }
    }
    
    LOG_INFO("Loaded {} users from file", m_users.size());
}
