#include "TcpServer.h"
#include "ConnectionHandler.h"

#include <QTcpSocket>
#include <QThread>
#include <QDebug>

// TCP 业务服务：每来一个连接，把 socket 移入专属线程，由 ConnectionHandler
// 在事件循环中按 4字节大端长度头 + JSON 逐帧处理。
// 会话校验/分发逻辑见 ConnectionHandler.cpp。
TcpServer::TcpServer(QObject* parent) : QObject(parent) {
    connect(&m_server, &QTcpServer::newConnection, this, &TcpServer::onNewConnection);
}

bool TcpServer::start(quint16 port) {
    m_port = port;
    if (!m_server.listen(QHostAddress::Any, m_port)) {
        qWarning() << "[TcpServer] listen failed:" << m_server.errorString();
        return false;
    }
    emit listeningChanged(true);
    emit logMessage(QStringLiteral("[服务] TCP 监听已启动：0.0.0.0:%1").arg(m_port));
    return true;
}

void TcpServer::stop() {
    if (m_server.isListening()) m_server.close();
    emit listeningChanged(false);
    emit logMessage(QStringLiteral("[服务] TCP 监听已停止"));
}

bool TcpServer::isListening() const { return m_server.isListening(); }
quint16 TcpServer::port() const { return m_port; }

void TcpServer::onNewConnection() {
    while (m_server.hasPendingConnections()) {
        QTcpSocket* conn = m_server.nextPendingConnection();
        const QString peer = conn->peerAddress().toString();

        emit logMessage(QStringLiteral("[连接] %1 已接入").arg(peer));
        emit clientConnected(peer);

        auto* handler = new ConnectionHandler(conn);
        auto* thread = new QThread(this);
        handler->moveToThread(thread);

        // 连接断开 -> 退出该线程并回收
        connect(handler, &ConnectionHandler::finished, thread, &QThread::quit);
        connect(handler, &ConnectionHandler::finished, handler, &QObject::deleteLater);
        connect(handler, &ConnectionHandler::finished, this,
                [this, peer]() {
                    emit clientDisconnected(peer);
                    emit logMessage(QStringLiteral("[连接] %1 已断开").arg(peer));
                });
        connect(thread, &QThread::finished, thread, &QObject::deleteLater);
        connect(thread, &QThread::finished, handler, &QObject::deleteLater);

        thread->start();
    }
}
