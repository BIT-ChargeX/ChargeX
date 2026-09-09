#pragma once
#include <QWidget>
#include <QJsonObject>
#include <QDateTime>

class QLabel;
class QPushButton;
class QCheckBox;
class QDateTimeEdit;
class QTimer;

// 充电业务模块-需求8/9/10：未完成订单检测 / 充电预约 / 充电订单生成
// 负责人：孙晟云   命令：ORDER_CHECK_UNFINISHED / ORDER_RESERVE / ORDER_CREATE
// 流程：
//   立即开始：预约(now) -> 生成订单 -> 充电中
//   预约到指定时间：预约(未来时间) -> 到点后手动开始 -> 充电中
class ChargingFlowWidget : public QWidget {
    Q_OBJECT
public:
    explicit ChargingFlowWidget(QWidget* parent = nullptr);

public slots:
    void onTabEntered();                                  // 主页切到充电页时自动检测
    void startChargingWithPile(const QJsonObject& pile);  // 找桩页选中电桩后进入
    void goPickPile();                                    // 未选桩时引导去"找桩"页

signals:
    void settleRequested(int orderId);        // 有未完成订单，请求打开结算页
    void goPickPileRequested();               // 请求切到找桩 Tab

private slots:
    void onActionClicked();       // 主按钮：立即开始 / 提交预约 / 到点开始充电
    void onSettleClicked();       // 结算未完成订单
    void onCancelClicked();       // 取消预约
    void onScheduleToggled(bool checked);

private:
    void doReserve();
    void createOrder(int pileId);
    void cancelReservation();
    void checkUnfinishedOrder();
    void render();                // 根据当前状态统一刷新按钮与提示
    void setStatus(const QString& text, bool ok);

    QLabel* m_statusLabel;
    QLabel* m_pileLabel;
    QCheckBox* m_scheduleCheck;
    QDateTimeEdit* m_timeEdit;
    QPushButton* m_actionBtn;     // 主操作按钮
    QPushButton* m_cancelBtn;     // 取消预约
    QPushButton* m_settleBtn;     // 结算
    QPushButton* m_goPickPileBtn;
    QTimer* m_countdownTimer;     // 预约到点后刷新状态

    QJsonObject m_pendingPile;    // 从站点详情页选中的电桩
    int m_unfinishedOrderId = 0;
    int m_unfinishedPileId = 0;
    QString m_unfinishedStatus;   // 未完成订单状态（预约占用/充电中/待结算）
    QDateTime m_reserveTime;      // 预约占用订单的计划开始时间
    bool m_busy = false;
};
