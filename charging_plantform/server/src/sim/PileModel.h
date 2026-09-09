#pragma once
#include <QString>
#include <QJsonObject>
#include <QtGlobal>

// server 内置“充电桩模拟模型”（并入 ChargingServer，由 server 代为管理）。
// 语义：绑定桩若 DB 状态=在用 视为“会话续跑”直接输出功率；否则功率仅在收到
// START（真实下单启动充电）后输出；故障/闲置功率恒 0。
class PileModel {
public:
    void setStatic(int pileId, const QString& code, const QString& type,
                   double power, const QString& status);

    void tick(int seconds);
    void apply(const QString& cmd, const QJsonObject& data);
    QJsonObject report() const;
    // 受控随机故障入口（由引擎统一决定频率与并发上限，避免随机堆满故障）
    void forceFault();

    int pileId() const { return m_pileId; }
    QString status() const { return m_status; }
    QString code() const { return m_code; }

private:
    void beginSession();
    void endSession();

    int m_pileId = 0;
    QString m_code;
    QString m_type;
    double m_power = 0.0;
    QString m_status;
    double m_capacityKwh = 15.0;
    double m_soc = 50.0;
    double m_curPowerKw = 0.0;
    int m_totalTimes = 0;
    double m_totalHours = 0.0;
    qint64 m_sessionStartMs = 0;   // 本次充电开始时刻(epoch ms)，由终端模拟记录；无会话=0
    int m_durationSec = 0;         // 本次充电计划时长(秒)，START 时随机 5~10 分钟
    int m_elapsedSec = 0;          // 已充时长(秒)
    double m_startSoc = 50.0;      // 本单起始 SOC
    bool m_chargeDone = false;     // 本次充电已结束（功率已回 0，等待结算）
};
