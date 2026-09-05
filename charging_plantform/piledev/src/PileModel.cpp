#include "PileModel.h"
#include <QRandomGenerator>
#include <QDateTime>
#include <QtMath>

void PileModel::setStatic(int pileId, const QString& code, const QString& type,
                          double power, const QString& status) {
    m_pileId = pileId;
    m_code = code;
    m_type = type;
    m_power = power;
    m_status = status;
    m_capacityKwh = power >= 50.0 ? 60.0 : 15.0;   // 快充/慢充容量简化假设
    m_soc = 20.0 + QRandomGenerator::global()->bounded(70);   // 20~90
    m_curPowerKw = 0.0;
}

void PileModel::setFault() {
    m_status = QStringLiteral("故障");
    m_curPowerKw = 0.0;
}

void PileModel::setIdle() {
    m_status = QStringLiteral("闲置");
    m_curPowerKw = 0.0;
}

void PileModel::tick(int seconds) {
    // 充电中：累计时长并按功率提升 SOC
    if (m_status == QStringLiteral("在用") && m_curPowerKw > 0.0) {
        const double dt = seconds / 3600.0;
        m_totalHours += dt;
        const double kwh = m_curPowerKw * dt;
        m_soc = qMin(100.0, m_soc + kwh / m_capacityKwh * 100.0);
        if (m_soc >= 100.0) m_curPowerKw = 0.0;   // 充满停止输出
    }

    // 空闲桩小概率随机故障，模拟真实设备故障（默认每 1000 次约 12 次，便于演示看到变化）
    if (m_status == QStringLiteral("闲置")
        && QRandomGenerator::global()->bounded(1000) < 12) {
        setFault();
    }
}

void PileModel::apply(const QString& cmd, const QJsonObject& data) {
    if (cmd == QStringLiteral("START")) {
        m_status = QStringLiteral("在用");
        m_curPowerKw = m_power;
    } else if (cmd == QStringLiteral("STOP")) {
        setIdle();
    } else if (cmd == QStringLiteral("REBOOT")) {
        setIdle();   // 重启成功：故障桩恢复闲置
    } else if (cmd == QStringLiteral("SET_STATUS")) {
        const QString want = data.value("status").toString();
        if (want == QStringLiteral("故障")) setFault();
        else if (want == QStringLiteral("闲置")) setIdle();
    }
}

QJsonObject PileModel::report() const {
    QJsonObject o;
    o["pile_id"] = m_pileId;
    o["status"] = m_status;
    o["soc"] = qRound(m_soc);
    o["cur_power"] = m_curPowerKw;
    o["total_times"] = m_totalTimes;
    o["total_hours"] = qRound(m_totalHours * 100.0) / 100.0;
    o["ts"] = QDateTime::currentDateTime().toString(Qt::ISODate);
    return o;
}
