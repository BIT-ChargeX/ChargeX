#include "PileModel.h"
#include <QRandomGenerator>
#include <QDateTime>
#include <QtMath>
#include <cmath>

namespace {
constexpr double kTaperSoc = 80.0;   // SOC 达到此值后开始降功率
constexpr double kChaseTau = 6.0;    // 功率趋近时间常数(秒)：5s一拍约4个点从0升到~95%额定
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
    // DB 在用 = 正在充电（会话续跑）：绑定即按当前 SOC 输出目标功率并记录会话起点；
    // 闲置/故障不输出，等待收到 START/指令
    if (status == QStringLiteral("在用")) {
        m_curPowerKw = targetPower();
        m_sessionStartMs = QDateTime::currentMSecsSinceEpoch();
    } else {
        m_curPowerKw = 0.0;
        m_sessionStartMs = 0;
    }
}

// 目标功率：SOC<80% → 额定；≥80% 按(100-soc)/(100-80)线性降到 100% 时 0
double PileModel::targetPower() const {
    if (m_soc >= 100.0) return 0.0;
    if (m_soc >= kTaperSoc) {
        const double factor = (100.0 - m_soc) / (100.0 - kTaperSoc);
        return m_power * factor;
    }
    return m_power;
}

void PileModel::tick(int seconds) {
    if (m_status != QStringLiteral("在用")) return;

    // 功率沿目标曲线“指数趋近”：开始从 0 爬升到额定；充满前随 SOC 上升降到 0
    const double target = targetPower();
    const double k = seconds / kChaseTau;
    const double decay = std::exp(-k);
    double next = target + (m_curPowerKw - target) * decay;
    if (next < 0.0) next = 0.0;
    m_curPowerKw = next;

    // 稳定阶段（≈额定且未进入降功率区）加 ±3% 微波动，让实时曲线可见
    const double dev = std::fabs(m_curPowerKw - m_power);
    if (m_soc < kTaperSoc && dev < m_power * 0.05) {
        m_curPowerKw = m_power
            * (0.97 + QRandomGenerator::global()->generateDouble() * 0.06);
    }

    const double dt = seconds / 3600.0;
    m_totalHours += dt;
    const double kwh = m_curPowerKw * dt;
    m_soc = qMin(100.0, m_soc + kwh / m_capacityKwh * 100.0);
    if (m_soc >= 100.0) {
        m_soc = 100.0;
        m_curPowerKw = 0.0;   // 充满归 0
    }
}

void PileModel::forceFault() {
    m_status = QStringLiteral("故障");
    m_curPowerKw = 0.0;
    m_sessionStartMs = 0;
}

void PileModel::apply(const QString& cmd, const QJsonObject& data) {
    if (cmd == QStringLiteral("START")) {
        m_status = QStringLiteral("在用");
        m_curPowerKw = 0.0;   // 从 0 开始上升
        m_sessionStartMs = QDateTime::currentMSecsSinceEpoch();
    } else if (cmd == QStringLiteral("STOP")) {
        m_status = QStringLiteral("闲置");
        m_curPowerKw = 0.0;
        m_sessionStartMs = 0;
    } else if (cmd == QStringLiteral("REBOOT")) {
        m_status = QStringLiteral("闲置");
        m_curPowerKw = 0.0;
        m_sessionStartMs = 0;
    } else if (cmd == QStringLiteral("SET_STATUS")) {
        const QString want = data.value("status").toString();
        if (want == QStringLiteral("故障")) {
            m_status = QStringLiteral("故障");
            m_curPowerKw = 0.0;
            m_sessionStartMs = 0;
        } else if (want == QStringLiteral("闲置")) {
            m_status = QStringLiteral("闲置");
            m_curPowerKw = 0.0;
            m_sessionStartMs = 0;
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
    o["total_times"] = m_totalTimes;
    o["total_hours"] = qRound(m_totalHours * 100.0) / 100.0;
    o["ts"] = QDateTime::currentDateTime().toString(Qt::ISODate);
    return o;
}
