#pragma once
#include <QObject>
#include <QTcpSocket>
#include <QJsonObject>
#include <QTimer>
#include <QMap>
#include <functional>

// 公共通信模块：封装与服务端的 TCP + JSON 协议交互。
// 所有业务模块只通过本类收发请求，禁止各自 new QTcpSocket。
// 帧格式：4字节大端长度头 + UTF-8 JSON（见 interface_protocol.md 第1节）
class NetClient : public QObject {
    Q_OBJECT
public:
    enum class State { Disconnected, Connecting, Connected };

    static NetClient& instance();

    void connectToServer(const QString& host, quint16 port);
    void disconnectFromServer();

    State state() const;
    bool isConnected() const;

    using ResponseCallback =
        std::function<void(const QJsonObject& data, int code, const QString& msg)>;

    // 发送请求；seq 自增，响应回来后触发对应回调（线程安全：均在事件循环内）
    void sendRequest(const QString& cmd, const QJsonObject& data,
                     ResponseCallback cb = nullptr);

signals:
    void connected();
    void disconnected();
    void stateChanged(int state);            // State 枚举值
    void errorOccurred(const QString& err);  // 连接失败/断开原因

private slots:
    void onReadyRead();
    void onSocketConnected();
    void onSocketDisconnected();
    void onSocketError();
    void onReconnectTick();

private:
    explicit NetClient(QObject* parent = nullptr);

    QTcpSocket m_socket;
    QByteArray m_buffer;
    int m_seqCounter = 1000;
    QMap<int, ResponseCallback> m_pending;

    QString m_host;
    quint16 m_port = 0;
    bool m_manualConnect = false;
    QTimer m_reconnectTimer;

    void tryConnect();
    void writeFrame(const QJsonObject& obj);
    void emitStateChanged();
};
