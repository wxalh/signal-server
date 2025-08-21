#include "config_util.h"
#include <QCoreApplication>
#include <QCryptographicHash>
/* spdlog头文件引用 */
#include "logger_manager.h"

ConfigUtilData::ConfigUtilData(QObject *parent)
    : QObject{parent}
{
    filePath = QCoreApplication::applicationDirPath() + "/config.ini";
    m_configIni = new QSettings(filePath, QSettings::IniFormat);
    m_configIni->setIniCodec("UTF-8");

    m_configIni->beginGroup("local");
    QString logLevelTmp = m_configIni->value("logLevel", "info").toString().toLower();
    m_configIni->endGroup();

    m_configIni->beginGroup("signal_server");
    serverPort = (quint16)(m_configIni->value("serverPort", 8080).toUInt());
    serverName = m_configIni->value("serverName", "Signal Server").toString();
    m_configIni->endGroup();

    if (logLevelTmp == "trace")
    {
        logLevel = spdlog::level::trace;
    }
    else if (logLevelTmp == "debug")
    {
        logLevel = spdlog::level::debug;
    }
    else if (logLevelTmp == "info")
    {
        logLevel = spdlog::level::info;
    }
    else if (logLevelTmp == "warn")
    {
        logLevel = spdlog::level::warn;
    }
    else if (logLevelTmp == "error")
    {
        logLevel = spdlog::level::err;
    }
    else if (logLevelTmp == "critical")
    {
        logLevel = spdlog::level::critical;
    }
    else
    {
        logLevel = spdlog::level::info; // 默认级别
    }
}

ConfigUtilData::~ConfigUtilData()
{
}

ConfigUtilData *ConfigUtilData::getInstance()
{
    static ConfigUtilData configUtil;
    return &configUtil;
}
