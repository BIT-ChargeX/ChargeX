#pragma once
#include <QWidget>
#include <QJsonObject>

class QLabel;
class QPushButton;
class QComboBox;

// 充电业务模块-需求8/9/10：未完成订单检测 / 充电预约 / 充电订单生成
// 负责人：孙晟云   命令：ORDER_CHECK_UNFINISHED / ORDER_RESERVE / ORDER_CREATE
// 流程：进入充电页 -> 自动检测未完成订单
//        有 -> 提示并显示"去结算"快捷入口（结算主入口已迁至"我的-我的订单"）
//        无 -> 选择电桩(由找桩页传入) -> 预约 -> 生成订单 -> 提示去结算
class ChargingFlowWidget : public QWidget {
    Q_OBJECT
public:
    explicit ChargingFlowWidget(QWidget* parent = nullptr);

public slots:
    void onTabEntered();                          // 主页切到充电页时自动检测
    void startChargingWithPile(const QJsonObject& pile); // 找桩页选中电桩后进入
    void goPickPile();                            // 未选桩时引导去"找桩"页

signals:
    void settleRequested(int orderId);            // 有未完成订单，请求打开结算页
    void goPickPileRequested();                   // 请求切到找桩 Tab

private slots:
    void onReserveClicked();
    void onSettleClicked();

private:
    void createOrder(int pileId);
    void doReserve();
    void checkUnfinishedOrder();
    void setStatus(const QString& text, bool ok);

    QLabel* m_statusLabel;
    QLabel* m_pileLabel;
    QComboBox* m_timeSlotCombo;
    QPushButton* m_reserveBtn;
    QPushButton* m_settleBtn;
    QPushButton* m_goPickPileBtn;

    QJsonObject m_pendingPile;   // 从站点详情页选中的电桩
    int m_unfinishedOrderId = 0;
    bool m_busy = false;
};
