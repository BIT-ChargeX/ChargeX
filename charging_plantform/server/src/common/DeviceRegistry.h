#pragma once
#include <QString>
#include <QSet>
#include <QHash>
#include <QList>
#include <QReadWriteLock>
#include <QJsonObject>
#include <QJsonArray>

// 服务器→充电桩终端 的控制指令
namespace DeviceCmd {
inline constexpr const char* kStart     = "START";
inline constexpr const char* kStop      = "STOP";
inline constexpr const char* kReboot    = "REBOOT";
inline constexpr const char* kSetStatus = "SET_STATUS";
}

// 充电桩模拟端注册表（线程安全）：
//  - device_id ↔ 绑定的电桩集合
//  - 每桩一张“待下发控制指令”队列（START/STOP/REBOOT/SET_STATUS）
// 控制指令由业务服务在落库成功后入队；设备下轮 REPORT 时取走，执行后 RESULT 回执清除。
class DeviceRegistry {
public:
    static DeviceRegistry& instance();

    // 分配绑定：设备已有绑定则返回现有；否则在 available(候选桩ID，升序)中
    // 选取未绑定的 want 台。返回最终绑定列表。
    QList<int> allocate(const QString& deviceId, int want, const QList<int>& available);

    QList<int> devicePiles(const QString& deviceId) const;
    bool isBound(int pileId) const;
    int boundCount() const;

    // 入队控制指令：电桩未绑定任何设备时直接丢弃并返回 false（服务器直改库的回退路径）
    bool enqueue(int pileId, const QString& cmd, const QJsonObject& data = QJsonObject());

    // 取走(不删除，等待回执)指定电桩的待办指令
    QJsonArray takePending(const QList<int>& pileIds);

    // 设备回执后清除待办
    void ackPending(int cmdId);

private:
    struct Pending {
        int pileId = 0;
        QString cmd;
        QJsonObject data;
    };

    DeviceRegistry() = default;

    mutable QReadWriteLock m_lock;
    QHash<QString, QList<int>> m_binding;
    QSet<int> m_bound;                 // 已被任一设备绑定
    int m_seq = 1000;
    QHash<int, Pending> m_pending;     // cmdId -> Pending
    QHash<int, QList<int>> m_queue;    // pileId -> [cmdId...]（保序）
};
