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
    logLevel = m_configIni->value("logLevel", "info").toString();
    m_configIni->endGroup();

    m_configIni->beginGroup("signal_server");
    serverPort = (quint16)(m_configIni->value("serverPort", 8080).toUInt());
    serverName = m_configIni->value("serverName", "Signal Server").toString();
    m_configIni->endGroup();
}

ConfigUtilData::~ConfigUtilData()
{
}

ConfigUtilData *ConfigUtilData::getInstance()
{
    static ConfigUtilData configUtil;
    return &configUtil;
}
