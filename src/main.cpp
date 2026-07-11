#include "config_util.h"
#include "logger_manager.h"
#include "usermanager.h"
#include "websocketserver.h"

#include <filesystem>
#include <iostream>
#include <string_view>
#ifdef _WIN32
#include <Windows.h>
#else
#include <fcntl.h>
#include <sys/file.h>
#include <unistd.h>
#endif

namespace {
bool isVersionArgument(std::string_view argument)
{
    return argument == "-v" || argument == "-V" || argument == "--version" || argument == "-version";
}

std::filesystem::path applicationDirectory(const char* argv0)
{
    std::error_code error;
    auto path = std::filesystem::absolute(argv0, error);
    return error ? std::filesystem::current_path() : path.parent_path();
}

bool isRunning()
{
#ifdef _WIN32
    static HANDLE mutex = CreateMutexW(nullptr, TRUE, L"Global\\airan_signal_server");
    return GetLastError() == ERROR_ALREADY_EXISTS;
#else
    static int lockFile = open("/tmp/airan_signal_server.lock", O_RDWR | O_CREAT, 0644);
    return lockFile != -1 && flock(lockFile, LOCK_EX | LOCK_NB) == -1;
#endif
}
}

int main(int argc, char* argv[])
{
    if (argc == 2 && isVersionArgument(argv[1])) {
        std::cout << "signal_server " << SIGNAL_SERVER_VERSION << '\n';
        return 0;
    }

#ifdef _WIN32
    SetConsoleOutputCP(CP_UTF8);
    SetConsoleCP(CP_UTF8);
#endif
    if (isRunning()) return 0;

    const auto appDir = applicationDirectory(argv[0]);
    ConfigUtil->load(appDir);
    LoggerManager::instance().initialize();
    LOG_INFO("Signal Server version {}", SIGNAL_SERVER_VERSION);
    UserManager::instance().initialize(appDir);

    auto port = ConfigUtil->serverPort == 0 ? 8080 : ConfigUtil->serverPort;
    WebSocketServer server(ConfigUtil->serverName, port);
    if (!server.start()) return 1;

    LOG_INFO("Server Name: {}", server.getServerName());
    LOG_INFO("WebSocket URL: ws://localhost:{}", server.getPort());
    server.run();
    LOG_INFO("Signal Server stopped");
    return 0;
}
