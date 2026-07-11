#pragma once

#include <cstdint>
#include <filesystem>
#include <string>
#include <spdlog/spdlog.h>

#define ConfigUtil ConfigUtilData::getInstance()

class ConfigUtilData {
public:
    static ConfigUtilData* getInstance();
    void load(const std::filesystem::path& applicationDir);

    std::filesystem::path filePath;
    std::uint16_t serverPort = 8080;
    std::string serverName = "Signal Server";
    spdlog::level::level_enum logLevel = spdlog::level::info;

private:
    ConfigUtilData() = default;
};
