#pragma once
#include <QObject>
#include <QTcpServer>

// TCP 业务服务：监听固定端口，每来一个连接起一个独立工作线程，
// 在线程内按 4字节大端长度头 + JSON 解析消息，再分发给各业务模块。
// 多线程满足课程要求；SQLite 连接按线程独立，见 DbManager::threadDb()。
class TcpServer : public QObject {
    Q_OBJECT
public:
    explicit TcpServer(QObject* parent = nullptr);

    bool start(quint16 port);
    void stop();
    bool isListening() const;
    quint16 port() const;

signals:
    void listeningChanged(bool listening);
    void clientConnected(const QString& peer);
    void clientDisconnected(const QString& peer);
    void logMessage(const QString& line);

private slots:
    void onNewConnection();

private:
    QTcpServer m_server;
    quint16 m_port = 0;
};
