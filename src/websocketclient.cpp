#include "websocketclient.h"
#include "logger_manager.h"
#include <QJsonDocument>

WebSocketClient::WebSocketClient(QWebSocket *socket, QObject *parent)
    : QObject(parent)
    , m_socket(socket)
    , m_remoteAddress(socket->peerAddress())
{
    // Connect socket signals
    connect(m_socket, &QWebSocket::textMessageReceived,
            this, &WebSocketClient::onTextMessageReceived);
    connect(m_socket, &QWebSocket::disconnected,
            this, &WebSocketClient::onDisconnected);
    connect(m_socket, QOverload<QAbstractSocket::SocketError>::of(&QWebSocket::error),
            this, &WebSocketClient::onSocketError);
    
    // Set socket parent to this client for proper cleanup
    m_socket->setParent(this);
}

WebSocketClient::~WebSocketClient()
{
    if (m_socket && m_socket->state() == QAbstractSocket::ConnectedState) {
        m_socket->close();
    }
}

bool WebSocketClient::isConnected() const
{
    return m_socket && m_socket->state() == QAbstractSocket::ConnectedState;
}

void WebSocketClient::sendMessage(const QString &message)
{
    if (isConnected()) {
        m_socket->sendTextMessage(message);
    } else {
        LOG_WARN("Attempting to send message to disconnected client: {}", m_sessionId);
        emit error("Client not connected");
    }
}

void WebSocketClient::sendJsonMessage(const QJsonObject &json)
{
    QJsonDocument doc(json);
    sendMessage(doc.toJson(QJsonDocument::Compact));
}

void WebSocketClient::onTextMessageReceived(const QString &message)
{
    emit messageReceived(message);
}

void WebSocketClient::onDisconnected()
{
    LOG_DEBUG("Client disconnected: {}", m_sessionId);
    emit disconnected();
}

void WebSocketClient::onSocketError()
{
    QString errorString = m_socket->errorString();
    LOG_WARN("WebSocket error for client {}: {}", m_sessionId, errorString);
    emit error(errorString);
}
