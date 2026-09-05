#pragma once
#include <QString>
#include <QJsonObject>

// 单台充电桩的“物理层”模拟：状态机 + SOC/功率推进 + 随机故障注入。
// 说明：服务器是业务/状态真源，本模型负责模拟真实世界表现并上报遥测；
//       “预约占用”由服务器维护，不在此模型内体现。
class PileModel {
public:
    PileModel() = default;

    void setStatic(int pileId, const QString& code, const QString& type,
                   double power, const QString& status);

    int pileId() const { return m_pileId; }
    QString code() const { return m_code; }
    QString status() const { return m_status; }

    // 每次上报前推进模拟（充电时长/电量，空闲桩随机故障）
    void tick(int seconds);

    // 执行服务器下发的控制指令
    void apply(const QString& cmd, const QJsonObject& data);

    QJsonObject report() const;

private:
    void setFault();
    void setIdle();

    int m_pileId = 0;
    QString m_code;
    QString m_type;
    double m_power = 0.0;
    double m_capacityKwh = 60.0;
    double m_soc = 50.0;
    double m_curPowerKw = 0.0;
    int m_totalTimes = 0;
    double m_totalHours = 0.0;
    QString m_status = QStringLiteral("闲置");
};
