#include <QCoreApplication>
#include <QDir>
#include "websocketserver.h"
#include "logger_manager.h"
#include "config_util.h"
/**
 * @brief isRunning 禁止多开
 * @return
 */
#ifdef Q_OS_WINDOWS
#include <Windows.h>
#else
#include <sys/file.h>
#include <fcntl.h>
#include <unistd.h>
#endif
bool isRunning()
{
#ifdef Q_OS_WINDOWS
    // Windows 使用互斥锁
    HANDLE hMutex = CreateMutexW(NULL, TRUE, L"Global\\airan_signal_server");
    if (GetLastError() == ERROR_ALREADY_EXISTS)
    {
        CloseHandle(hMutex);
        return true;
    }
    return false;
#else
    // Linux/macOS 使用文件锁
    static int lockFile = -1;
    if (lockFile == -1)
    {
        QString lockPath = QDir::temp().absoluteFilePath("airan_signal_server.lock");
        lockFile = open(lockPath.toLocal8Bit().constData(), O_RDWR | O_CREAT, 0644);
    }

    if (lockFile != -1 && flock(lockFile, LOCK_EX | LOCK_NB) == -1)
    {
        return true; // 已锁定表示程序已在运行
    }
    return false;
#endif
}

int main(int argc, char *argv[])
{
    QCoreApplication app(argc, argv);
    if (isRunning())
    {
        return 0;
    }
    // Set application information
    app.setApplicationName("Signal Server");
    app.setApplicationVersion("1.0.0");
    app.setOrganizationName("WXALH");
    app.setOrganizationDomain("wxalh.com");
    
    // Initialize configuration first
    ConfigUtil; // This will initialize the singleton and load config.ini
    
    // Get server configuration from config file
    quint16 port = ConfigUtil->serverPort;
    QString serverName = ConfigUtil->serverName;
    
    // Validate configuration
    if (port == 0 || port > 65535) {
        LOG_ERROR("Invalid port number: {}, using default 8080", port);
        port = 8080;
    }
    
    if (serverName.isEmpty()) {
        serverName = "Signal Server";
    }
    
    LOG_INFO("Starting server with configuration:");
    LOG_INFO("  Server Name: {}", serverName);
    LOG_INFO("  Port: {}", port);
    LOG_INFO("  Config file: {}", ConfigUtil->filePath);
    
    // Create and start server
    WebSocketServer server(serverName, port);
    
    // Connect server signals for logging
    QObject::connect(&server, &WebSocketServer::serverStarted, [&]() {
        LOG_INFO("=== Signal Server Started ===");
        LOG_INFO("Server Name: {}", server.getServerName());
        LOG_INFO("Listening Port: {}", server.getPort());
        LOG_INFO("WebSocket URL: ws://localhost:{}", server.getPort());
        LOG_INFO("==============================");
    });
    
    QObject::connect(&server, &WebSocketServer::serverStopped, [&]() {
        LOG_INFO("=== Signal Server Stopped ===");
        app.quit();
    });
    
    QObject::connect(&server, &WebSocketServer::serverError, [&](const QString &error) {
        LOG_ERROR("Server Error: {}", error);
        app.quit();
    });
    
    QObject::connect(&server, &WebSocketServer::clientConnected, [&](WebSocketClient *client) {
        LOG_INFO("Client connected: {} from {} ({})", 
                 client->getSessionId(), 
                 client->getRemoteAddress().toString(),
                 client->getHostname());
    });
    
    QObject::connect(&server, &WebSocketServer::clientDisconnected, [&](WebSocketClient *client) {
        LOG_INFO("Client disconnected: {}", client->getSessionId());
    });
    
    // Start the server
    if (!server.start()) {
        LOG_ERROR("Failed to start server");
        return 1;
    }
    
    // Handle graceful shutdown
    QObject::connect(&app, &QCoreApplication::aboutToQuit, [&]() {
        LOG_INFO("Shutting down server...");
        server.stop();
    });
    
    LOG_INFO("Press Ctrl+C to stop the server");
    
    return app.exec();
}
