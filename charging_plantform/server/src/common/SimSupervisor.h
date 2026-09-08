#pragma once
#include <QObject>

class QProcess;

// 托管充电桩模拟终端：ChargingServer 启动时拉起 ChargingPileSim 子进程，
// 异常退出自动重启（2s 退避），Server 退出时终止子进程。
class SimSupervisor : public QObject {
    Q_OBJECT
public:
    explicit SimSupervisor(QObject* parent = nullptr);

    // bin: ChargingPileSim 可执行文件路径；pileCount<=0 时不启动
    void configure(const QString& bin, const QString& host, quint16 port,
                   const QString& deviceId, int pileCount);

    void start();
    void stop();

private:
    void launch();

    QProcess* m_proc = nullptr;
    QString m_bin;
    QString m_host;
    QString m_deviceId;
    quint16 m_port = 0;
    int m_piles = 0;
    bool m_started = false;
    bool m_shutting = false;
};
