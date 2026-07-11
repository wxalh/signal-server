#include "config_util.h"

#include <algorithm>
#include <cctype>
#include <fstream>
#include <unordered_map>

ConfigUtilData* ConfigUtilData::getInstance()
{
    static ConfigUtilData instance;
    return &instance;
}

void ConfigUtilData::load(const std::filesystem::path& applicationDir)
{
    filePath = applicationDir / "config.ini";
    std::ifstream input(filePath);
    std::unordered_map<std::string, std::string> values;
    std::string section;
    std::string line;
    while (std::getline(input, line)) {
        line.erase(std::remove(line.begin(), line.end(), '\r'), line.end());
        const auto first = line.find_first_not_of(" \t");
        if (first == std::string::npos || line[first] == '#' || line[first] == ';') continue;
        if (line[first] == '[') {
            const auto end = line.find(']', first + 1);
            if (end != std::string::npos) section = line.substr(first + 1, end - first - 1);
            continue;
        }
        const auto equal = line.find('=', first);
        if (equal == std::string::npos) continue;
        auto key = line.substr(first, equal - first);
        auto value = line.substr(equal + 1);
        key.erase(key.find_last_not_of(" \t") + 1);
        const auto valueFirst = value.find_first_not_of(" \t");
        value = valueFirst == std::string::npos ? "" : value.substr(valueFirst);
        values[section + "." + key] = value;
    }

    if (auto it = values.find("signal_server.serverPort"); it != values.end()) {
        try {
            const auto port = std::stoul(it->second);
            if (port <= 65535) serverPort = static_cast<std::uint16_t>(port);
        } catch (...) {}
    }
    if (auto it = values.find("signal_server.serverName"); it != values.end() && !it->second.empty()) serverName = it->second;

    auto level = values.count("local.logLevel") ? values["local.logLevel"] : "info";
    std::transform(level.begin(), level.end(), level.begin(), [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
    if (level == "trace") logLevel = spdlog::level::trace;
    else if (level == "debug") logLevel = spdlog::level::debug;
    else if (level == "warn") logLevel = spdlog::level::warn;
    else if (level == "error") logLevel = spdlog::level::err;
    else if (level == "critical") logLevel = spdlog::level::critical;
    else logLevel = spdlog::level::info;
}
