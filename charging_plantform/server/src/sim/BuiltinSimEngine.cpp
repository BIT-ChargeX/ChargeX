#include "BuiltinSimEngine.h"
#include "PileModel.h"
#include "service/PileDeviceService.h"

#include <QTimer>
#include <QJsonArray>
#include <QJsonObject>
#include <QRandomGenerator>
#include <QDebug>

namespace {
// 受控随机故障：极低概率 + 同时故障数上限，避免“全库堆成故障”
constexpr int kFaultDenominator = 10000;   // 每桩每次 5s 拍 1/10000 概率
constexpr int kMaxFaults = 1;
}

BuiltinSimEngine::BuiltinSimEngine(QObject* parent) : QObject(parent) {
    m_timer = new QTimer(this);
    m_timer->setInterval(5000);
    connect(m_timer, &QTimer::timeout, this, &BuiltinSimEngine::onTick);
}

BuiltinSimEngine::~BuiltinSimEngine() {
    qDeleteAll(m_models);
    m_models.clear();
}

void BuiltinSimEngine::configure(const QString& deviceId, int pileCount) {
    m_deviceId = deviceId;
    m_piles = qBound(0, pileCount, 200);
}

bool BuiltinSimEngine::start() {
    if (m_started) return true;
    if (m_piles <= 0) {
        qWarning().noquote() << QStringLiteral("[内置桩] pileCount=0，未启动");
        return false;
    }
    m_started = true;
    bind();
    m_timer->start();
    return true;
}

void BuiltinSimEngine::stop() {
    m_shutting = true;
    if (m_timer) m_timer->stop();
    qDeleteAll(m_models);
    m_models.clear();
}

void BuiltinSimEngine::bind() {
    QJsonObject data;
    data["device_id"] = m_deviceId;
    data["pile_count"] = m_piles;

    const Api::Reply r = PileDeviceService::hello(data);
    if (r.code != Api::Ok) {
        qWarning().noquote()
            << QStringLiteral("[内置桩] HELLO 失败：%1").arg(r.data.value("msg").toString());
        return;
    }

    const QJsonArray bound = r.data.value("bound").toArray();
    for (const auto& v : bound) {
        const QJsonObject b = v.toObject();
        auto* pm = new PileModel;
        pm->setStatic(b.value("pile_id").toInt(),
                      b.value("code").toString(),
                      b.value("type").toString(),
                      b.value("power").toDouble(),
                      b.value("status").toString());
        m_models.append(pm);
    }
    m_bound = !m_models.isEmpty();
    qInfo().noquote()
        << QStringLiteral("[内置桩] device=%1 绑定 %2 台").arg(m_deviceId).arg(m_models.size());
    if (!m_bound) {
        qWarning().noquote()
            << QStringLiteral("[内置桩] 无可绑定电桩（可能已被外部终端占用）");
        return;
    }

    // 绑定后立即补一帧：让 DB=在用 的桩曲线起点=server 启动时刻
    reportOnce(false);
}

void BuiltinSimEngine::onTick() {
    if (m_shutting) return;
    if (!m_bound) bind();   // 之前无可用桩时再试
    maybeRandomFault();
    reportOnce(true);
}

// 受控随机故障：统计内置桩当前故障数，未达上限时才以极低概率让一台闲置桩故障
void BuiltinSimEngine::maybeRandomFault() {
    int faultCount = 0;
    for (PileModel* pm : m_models)
        if (pm->status() == QStringLiteral("故障")) ++faultCount;
    if (faultCount >= kMaxFaults) return;

    for (PileModel* pm : m_models) {
        if (pm->status() != QStringLiteral("闲置")) continue;
        if (QRandomGenerator::global()->bounded(kFaultDenominator) < 1) {
            pm->forceFault();
            qInfo().noquote()
                << QStringLiteral("[内置桩] 桩 %1(%2) 随机故障")
                       .arg(pm->pileId()).arg(pm->code());
            return;   // 同一拍最多再故障一台
        }
    }
}

void BuiltinSimEngine::reportOnce(bool advanceTick) {
    if (m_shutting || m_models.isEmpty()) return;

    if (advanceTick) {
        for (PileModel* pm : m_models) pm->tick(5);
    }

    QJsonArray reports;
    for (PileModel* pm : m_models) reports.append(pm->report());

    QJsonObject data;
    data["device_id"] = m_deviceId;
    data["reports"] = reports;

    const Api::Reply r = PileDeviceService::report(data);
    if (r.code == Api::Ok) handlePending(r.data.value("pending").toArray());
}

void BuiltinSimEngine::handlePending(const QJsonArray& pending) {
    if (pending.isEmpty()) return;

    bool executed = false;
    for (const auto& v : pending) {
        const QJsonObject p = v.toObject();
        const int pileId = p.value("pile_id").toInt();
        const int cmdId = p.value("cmd_id").toInt();
        const QString cmd = p.value("cmd").toString();

        for (PileModel* pm : m_models) {
            if (pm->pileId() != pileId) continue;
            const QString before = pm->status();
            pm->apply(cmd, p.value("data").toObject());
            qInfo().noquote()
                << QStringLiteral("[内置桩] 桩 %1(%2) 执行 %3：%4 → %5")
                       .arg(pileId).arg(pm->code()).arg(cmd).arg(before).arg(pm->status());
            executed = true;

            QJsonObject res;
            res["device_id"] = m_deviceId;
            res["pile_id"] = pileId;
            res["cmd_id"] = cmdId;
            res["result"] = QStringLiteral("ok");
            res["detail"] = QStringLiteral("已执行%1").arg(cmd);
            PileDeviceService::result(res);
            break;
        }
    }

    // 指令执行后（尤其 START）立即补报一帧，使服务端首条功率采样≈充电启动时刻
    if (executed) reportOnce(false);
}
