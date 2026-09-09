#include "PileModel.h"
#include <QRandomGenerator>
#include <QDateTime>
#include <QtMath>
#include <cmath>

namespace {
constexpr double kTaperSec = 20.0;   // 剩余该秒数时开始线性降功率
constexpr double kChaseTau = 6.0;    // 功率趋近时间常数(秒)
constexpr int kMinChargeSec = 300;   // 单次充电最短时长 5 分钟
constexpr int kMaxChargeSec = 600;   // 单次充电最长时长 10 分钟

int randomChargeSec() {
    return kMinChargeSec + QRandomGenerator::global()->bounded(kMaxChargeSec - kMinChargeSec + 1);
}
}

void PileModel::beginSession() {
    m_sessionStartMs = QDateTime::currentMSecsSinceEpoch();
    m_durationSec = randomChargeSec();
    m_elapsedSec = 0;
    m_startSoc = m_soc;
    m_chargeDone = false;
    m_curPowerKw = 0.0;   // 从 0 开始爬升
}

void PileModel::endSession() {
    m_sessionStartMs = 0;
    m_durationSec = 0;
    m_elapsedSec = 0;
    m_chargeDone = false;
    m_curPowerKw = 0.0;
}

void PileModel::setStatic(int pileId, const QString& code, const QString& type,
                          double power, const QString& status) {
    m_pileId = pileId;
    m_code = code;
    m_type = type;
    m_power = power;
    m_status = status;
    m_capacityKwh = power >= 50.0 ? 60.0 : 15.0;   // 快充/慢充容量简化
    m_soc = 20.0 + QRandomGenerator::global()->bounded(70);   // 20~90
    if (status == QStringLiteral("在用")) {
        // DB 在用 = 会话从此刻开始（终端模拟），时长随机 5~10 分钟
        beginSession();
    } else {
        m_curPowerKw = 0.0;
        m_sessionStartMs = 0;
        m_durationSec = 0;
        m_elapsedSec = 0;
        m_chargeDone = false;
    }
}

void PileModel::tick(int seconds) {
    if (m_status != QStringLiteral("在用")) return;
    if (m_chargeDone) { m_curPowerKw = 0.0; return; }

    m_elapsedSec += seconds;
    if (m_elapsedSec >= m_durationSec) {
        m_chargeDone = true;
        m_soc = 100.0;
        m_curPowerKw = 0.0;   // 充完 → 功率回落 0，等待结算
        return;
    }

    // SOC 随时间线性充到 100%
    m_soc = qMin(100.0, m_startSoc + (100.0 - m_startSoc) * m_elapsedSec / m_durationSec);

    // 功率：剩余≤20s 线性下降，否则按额定趋近（0→额定爬升）
    const double remaining = m_durationSec - m_elapsedSec;
    const double target = remaining <= kTaperSec
                              ? m_power * remaining / kTaperSec
                              : m_power;
    const double decay = std::exp(-seconds / kChaseTau);
    double next = target + (m_curPowerKw - target) * decay;
    if (next < 0.0) next = 0.0;
    m_curPowerKw = next;

    // 稳定段 ±3% 微波动
    const double dev = std::fabs(m_curPowerKw - m_power);
    if (remaining > kTaperSec && dev < m_power * 0.05) {
        m_curPowerKw = m_power
            * (0.97 + QRandomGenerator::global()->generateDouble() * 0.06);
    }

    m_totalHours += seconds / 3600.0;
}

void PileModel::forceFault() {
    m_status = QStringLiteral("故障");
    endSession();
}

void PileModel::apply(const QString& cmd, const QJsonObject& data) {
    if (cmd == QStringLiteral("START")) {
        m_status = QStringLiteral("在用");
        beginSession();
    } else if (cmd == QStringLiteral("STOP")) {
        m_status = QStringLiteral("闲置");
        endSession();
    } else if (cmd == QStringLiteral("REBOOT")) {
        m_status = QStringLiteral("闲置");
        endSession();
    } else if (cmd == QStringLiteral("SET_STATUS")) {
        const QString want = data.value("status").toString();
        if (want == QStringLiteral("故障")) {
            m_status = QStringLiteral("故障");
            endSession();
        } else if (want == QStringLiteral("闲置")) {
            m_status = QStringLiteral("闲置");
            endSession();
        }
    }
}

QJsonObject PileModel::report() const {
    QJsonObject o;
    o["pile_id"] = m_pileId;
    o["status"] = m_status;
    o["soc"] = qRound(m_soc);
    o["cur_power"] = m_curPowerKw;
    o["session_start_ms"] = m_sessionStartMs;
    o["charge_done"] = m_chargeDone;
    o["total_times"] = m_totalTimes;
    o["total_hours"] = qRound(m_totalHours * 100.0) / 100.0;
    o["ts"] = QDateTime::currentDateTime().toString(Qt::ISODate);
    return o;
}
