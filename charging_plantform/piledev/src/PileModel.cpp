#include "PileModel.h"
#include <QRandomGenerator>
#include <QDateTime>
#include <QtMath>
#include <cmath>

namespace {
constexpr double kTaperSec = 20.0;
constexpr double kChaseTau = 6.0;
constexpr int kMinChargeSec = 300;
constexpr int kMaxChargeSec = 600;

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
    m_capacityKwh = power >= 50.0 ? 60.0 : 15.0;
    m_soc = 20.0 + QRandomGenerator::global()->bounded(70);
    if (status == QStringLiteral("在用")) {
        beginSession();
    } else {
        m_curPowerKw = 0.0;
        m_sessionStartMs = 0;
        m_durationSec = 0;
        m_elapsedSec = 0;
        m_chargeDone = false;
    }
}

void PileModel::forceFault() {
    setFault();
}

void PileModel::setFault() {
    m_status = QStringLiteral("故障");
    endSession();
}

void PileModel::setIdle() {
    m_status = QStringLiteral("闲置");
    endSession();
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

    m_soc = qMin(100.0, m_startSoc + (100.0 - m_startSoc) * m_elapsedSec / m_durationSec);

    const double remaining = m_durationSec - m_elapsedSec;
    const double target = remaining <= kTaperSec
                              ? m_power * remaining / kTaperSec
                              : m_power;
    const double decay = std::exp(-seconds / kChaseTau);
    double next = target + (m_curPowerKw - target) * decay;
    if (next < 0.0) next = 0.0;
    m_curPowerKw = next;

    const double dev = std::fabs(m_curPowerKw - m_power);
    if (remaining > kTaperSec && dev < m_power * 0.05) {
        m_curPowerKw = m_power
            * (0.97 + QRandomGenerator::global()->generateDouble() * 0.06);
    }

    m_totalHours += seconds / 3600.0;
}

void PileModel::apply(const QString& cmd, const QJsonObject& data) {
    if (cmd == QStringLiteral("START")) {
        m_status = QStringLiteral("在用");
        beginSession();
    } else if (cmd == QStringLiteral("STOP")) {
        setIdle();
    } else if (cmd == QStringLiteral("REBOOT")) {
        setIdle();
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
    o["session_start_ms"] = m_sessionStartMs;
    o["charge_done"] = m_chargeDone;
    o["total_times"] = m_totalTimes;
    o["total_hours"] = qRound(m_totalHours * 100.0) / 100.0;
    o["ts"] = QDateTime::currentDateTime().toString(Qt::ISODate);
    return o;
}
