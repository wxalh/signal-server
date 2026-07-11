#pragma once

#include <memory>
#include <string>
#include <spdlog/spdlog.h>

class LoggerManager {
public:
    static LoggerManager& instance();
    void initialize(const std::string& logDirectory = {});
    std::shared_ptr<spdlog::logger> getLogger();

private:
    LoggerManager() = default;
    std::shared_ptr<spdlog::logger> m_logger;
};

#define LOG_TRACE(...) LoggerManager::instance().getLogger()->trace(__VA_ARGS__)
#define LOG_DEBUG(...) LoggerManager::instance().getLogger()->debug(__VA_ARGS__)
#define LOG_INFO(...) LoggerManager::instance().getLogger()->info(__VA_ARGS__)
#define LOG_WARN(...) LoggerManager::instance().getLogger()->warn(__VA_ARGS__)
#define LOG_WARNING(...) LoggerManager::instance().getLogger()->warn(__VA_ARGS__)
#define LOG_ERROR(...) LoggerManager::instance().getLogger()->error(__VA_ARGS__)
