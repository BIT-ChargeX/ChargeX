#include "DeviceRegistry.h"

DeviceRegistry& DeviceRegistry::instance() {
    static DeviceRegistry inst;
    return inst;
}

QList<int> DeviceRegistry::allocate(const QString& deviceId, int want,
                                    const QList<int>& available) {
    QWriteLocker lock(&m_lock);

    if (m_binding.contains(deviceId)) return m_binding.value(deviceId);

    QList<int> chosen;
    for (int id : available) {
        if (m_bound.contains(id)) continue;
        chosen.append(id);
        if (want > 0 && chosen.size() >= want) break;
    }
    for (int id : chosen) m_bound.insert(id);
    m_binding.insert(deviceId, chosen);
    return chosen;
}

QList<int> DeviceRegistry::devicePiles(const QString& deviceId) const {
    QReadLocker lock(&m_lock);
    return m_binding.value(deviceId);
}

bool DeviceRegistry::isBound(int pileId) const {
    QReadLocker lock(&m_lock);
    return m_bound.contains(pileId);
}

int DeviceRegistry::boundCount() const {
    QReadLocker lock(&m_lock);
    return m_bound.size();
}

bool DeviceRegistry::enqueue(int pileId, const QString& cmd, const QJsonObject& data) {
    QWriteLocker lock(&m_lock);
    if (!m_bound.contains(pileId)) return false;   // 未接入设备 → 回退：服务器已直改库

    const int cmdId = m_seq++;
    m_pending.insert(cmdId, Pending{pileId, cmd, data});
    m_queue[pileId].append(cmdId);
    return true;
}

QJsonArray DeviceRegistry::takePending(const QList<int>& pileIds) {
    QJsonArray arr;
    QReadLocker lock(&m_lock);
    int taken = 0;
    for (int pileId : pileIds) {
        const QList<int> ids = m_queue.value(pileId);
        for (int cmdId : ids) {
            if (taken >= 200) break;
            auto it = m_pending.constFind(cmdId);
            if (it == m_pending.constEnd()) continue;
            QJsonObject o;
            o["cmd_id"] = cmdId;
            o["pile_id"] = it.value().pileId;
            o["cmd"] = it.value().cmd;
            o["data"] = it.value().data;
            arr.append(o);
            ++taken;
        }
    }
    return arr;
}

void DeviceRegistry::ackPending(int cmdId) {
    QWriteLocker lock(&m_lock);
    auto it = m_pending.find(cmdId);
    if (it == m_pending.end()) return;
    const int pileId = it.value().pileId;
    m_pending.erase(it);
    if (m_queue.contains(pileId)) {
        QList<int>& ids = m_queue[pileId];
        ids.removeAll(cmdId);
        if (ids.isEmpty()) m_queue.remove(pileId);
    }
}
