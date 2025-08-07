#ifndef USERMANAGER_H
#define USERMANAGER_H

#include <QObject>
#include <QHash>
#include <QList>
#include <QMutex>
#include "rcsuser.h"

class UserManager : public QObject
{
    Q_OBJECT

public:
    explicit UserManager(QObject *parent = nullptr);
    ~UserManager();

    // User operations
    RcsUser* selectRcsUserBySN(const QString &sn);
    void insertRcsUser(const RcsUser &user);
    void updateRcsUser(const RcsUser &user);
    void deleteRcsUser(const QString &sn);
    
    // Query operations
    QList<RcsUser> selectRcsUserList(const RcsUser &criteria);
    QList<RcsUser> getAllOnlineUsers();
    int getOnlineUserCount();
    
    // Status operations
    void setUserOnline(const QString &sn);
    void setUserOffline(const QString &sn);
    bool isUserOnline(const QString &sn);

    // Singleton access
    static UserManager& instance();

signals:
    void userOnline(const RcsUser &user);
    void userOffline(const RcsUser &user);
    void userUpdated(const RcsUser &user);

private:
    QHash<QString, RcsUser> m_users;
    QMutex m_mutex;
    static UserManager* s_instance;
    
    void saveUserToFile(const RcsUser &user);
    void loadUsersFromFile();
    QString getUserDataFilePath() const;
};

#endif // USERMANAGER_H
