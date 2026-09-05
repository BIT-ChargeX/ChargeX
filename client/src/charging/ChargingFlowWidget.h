#pragma once
#include <QWidget>
#include <QJsonObject>

class QLabel;
class QPushButton;
class QComboBox;

// 充电业务模块-需求8/9/10：未完成订单检测 / 充电预约 / 充电订单生成
// 负责人：孙晟云   命令：ORDER_CHECK_UNFINISHED / ORDER_RESERVE / ORDER_CREATE
// 流程：进入充电页 -> 检测未完成订单
//        有 -> orderInterrupted(orderId)，主页弹窗 + 强制跳结算页
//        无 -> 选择电桩(由找桩页传入) -> 预约 -> 生成订单
class ChargingFlowWidget : public QWidget {
    Q_OBJECT
public:
    explicit ChargingFlowWidget(QWidget* parent = nullptr);

public slots:
    void onTabEntered();                          // 主页切到充电页时自动检测
    void startChargingWithPile(const QJsonObject& pile); // 找桩页选中电桩后进入
    void checkUnfinishedOrder();
    void goPickPile();                            // 未选桩时引导去"找桩"页

signals:
    void orderInterrupted(int orderId);           // 有未完成订单，主页弹窗并打开结算页
    void goPickPileRequested();                   // 请求切到找桩 Tab

private slots:
    void onReserveClicked();

private:
    void createOrder(int pileId);
    void doReserve();
    void setStatus(const QString& text, bool ok);

    QLabel* m_statusLabel;
    QLabel* m_pileLabel;
    QComboBox* m_timeSlotCombo;
    QPushButton* m_checkBtn;
    QPushButton* m_reserveBtn;
    QPushButton* m_settlementBtn;
    QPushButton* m_goPickPileBtn;

    QJsonObject m_pendingPile;   // 从站点详情页选中的电桩
    bool m_busy = false;
};
