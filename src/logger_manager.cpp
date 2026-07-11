#include "logger_manager.h"
#include "config_util.h"

#include <filesystem>
#include <spdlog/sinks/daily_file_sink.h>
#include <spdlog/sinks/stdout_color_sinks.h>

LoggerManager& LoggerManager::instance()
{
    static LoggerManager instance;
    return instance;
}

void LoggerManager::initialize(const std::string& logDirectory)
{
    if (m_logger) return;
    try {
        auto directory = logDirectory.empty() ? ConfigUtil->filePath.parent_path() / "logs" : std::filesystem::path(logDirectory);
        std::filesystem::create_directories(directory);
        auto console = std::make_shared<spdlog::sinks::stdout_color_sink_mt>();
        auto file = std::make_shared<spdlog::sinks::daily_file_sink_mt>((directory / "signal_server").string(), 0, 0);
        console->set_pattern("[%Y-%m-%d %H:%M:%S.%e] [%P/%t] [%^%l%$] %v");
        file->set_pattern("[%Y-%m-%d %H:%M:%S.%e] [%P/%t] [%l] %v");
        m_logger = std::make_shared<spdlog::logger>("signal_server", spdlog::sinks_init_list{console, file});
        m_logger->set_level(ConfigUtil->logLevel);
        m_logger->flush_on(spdlog::level::info);
        spdlog::set_default_logger(m_logger);
    } catch (const spdlog::spdlog_ex&) {
        m_logger = spdlog::stdout_color_mt("signal_server_fallback");
    }
}

std::shared_ptr<spdlog::logger> LoggerManager::getLogger()
{
    if (!m_logger) initialize();
    return m_logger;
}
