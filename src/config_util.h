#ifndef CONFIG_UTIL_H
#define CONFIG_UTIL_H

#include <QObject>
#include <QSettings>
#include <QUuid>

#define ConfigUtil ConfigUtilData::getInstance()

class ConfigUtilData : public QObject
{
    Q_OBJECT
public:
    explicit ConfigUtilData(QObject *parent = nullptr);
    ~ConfigUtilData();
    static ConfigUtilData* getInstance();
public:
    QString filePath;
    QSettings *m_configIni;
    // 服务器配置参数
    quint16 serverPort;
    QString serverName;

    QString logLevel;
signals:
};

#endif // CONFIG_UTIL_H
