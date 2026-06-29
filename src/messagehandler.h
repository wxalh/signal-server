#ifndef MESSAGEHANDLER_H
#define MESSAGEHANDLER_H

#include <QObject>
#include <QString>
#include <QJsonObject>
#include "wsmsg.h"

class WebSocketClient;
class WebSocketServer;

class MessageHandler : public QObject
{
    Q_OBJECT

public:
    explicit MessageHandler(WebSocketServer *server, QObject *parent = nullptr);

    // Message processing
    void handleMessage(WebSocketClient *client, const QString &message);
    void handleHeartbeat(WebSocketClient *client);
    void handleSignalMessage(WebSocketClient *client, const WsMsg &wsMsg, const QString &message);

private:
    WebSocketServer *m_server;

    void sendErrorMessage(WebSocketClient *client, const QString &errorMsg, const QString &type = "error");
    bool validateMessage(const WsMsg &wsMsg);
};

#endif // MESSAGEHANDLER_H
