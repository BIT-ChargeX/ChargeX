#pragma once
#include <QObject>
#include <QTcpSocket>
#include <QJsonObject>
#include <QJsonArray>
#include <QTimer>
#include <QVector>
#include <QHash>
#include <functional>

#include "PileModel.h"

// 充电桩模拟端网络客户端：连接服务器、HELLO 绑定、定时上报、执行并回执控制指令。
// 帧格式与服务器一致：4 字节大端长度头 + UTF-8 JSON。
class SimClient : public QObject {
    Q_OBJECT
public:
    explicit SimClient(const QString& host, quint16 port, const QString& deviceId,
                       int pileCount, QObject* parent = nullptr);

    void start();

signals:
    void logMessage(const QString& line);

private slots:
    void onConnected();
    void onDisconnected();
    void onReadyRead();
    void onReportTick();
    void onReconnectTick();

private:
    using Callback = std::function<void(const QJsonObject&, int, const QString&)>;

    void tryConnect();
    void sendRequest(const QString& cmd, const QJsonObject& data, Callback cb = nullptr);
    void sendHello();
    void doReport();
    void handlePending(const QJsonArray& pending);
    void writeFrame(const QJsonObject& obj);
    void log(const QString& line);

    QString m_host;
    quint16 m_port;
    QString m_deviceId;
    int m_pileCount;

    QTcpSocket m_socket;
    QByteArray m_buffer;
    int m_seq = 1000;
    QHash<int, Callback> m_pending;

    QTimer m_reportTimer;
    QTimer m_reconnectTimer;

    QVector<PileModel> m_models;
};
