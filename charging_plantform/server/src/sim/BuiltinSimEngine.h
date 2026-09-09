#pragma once
#include <QObject>
#include <QVector>
#include <QString>

class QTimer;
class PileModel;

// server 内置“充电桩模拟终端”引擎（方案甲）：
// 直接复用 PileDeviceService.hello/report/result 与 DeviceRegistry，
// 在 ChargingServer 进程内模拟 N 台桩（device_id=builtin），无需外部 ChargingPileSim 进程。
// 行为：HELLO 绑定 → 每 5s REPORT（带回 pending）→ 就地执行控制指令 → RESULT 回执；
//       绑定到“在用”桩即输出功率，收到 START 后立即补报一帧（起点≈充电启动）。
class BuiltinSimEngine : public QObject {
    Q_OBJECT
public:
    explicit BuiltinSimEngine(QObject* parent = nullptr);
    ~BuiltinSimEngine() override;

    void configure(const QString& deviceId, int pileCount);
    bool start();
    void stop();

private:
    void bind();
    void onTick();
    void maybeRandomFault();
    void reportOnce(bool advanceTick);
    void handlePending(const QJsonArray& pending);

    QTimer* m_timer = nullptr;
    QString m_deviceId = QStringLiteral("builtin");
    int m_piles = 0;
    bool m_bound = false;
    bool m_started = false;
    bool m_shutting = false;
    QVector<PileModel*> m_models;
};
