#pragma once
#include <QDialog>

class QLabel;
class QPushButton;
class QTimer;
class QHideEvent;

// 充电业务模块：未完成充电订单结算页（需求8 强制跳转目标）
// 结算/计费/扣款/释放电桩全部由服务端 ORDER_SETTLE 处理，本页只提交结算请求。
// 打开页面即拉取 ORDER_SETTLE_PREVIEW 预估金额（每 5 秒刷新，只读），结算前可见本次扣费。
class SettlementWidget : public QDialog {
    Q_OBJECT
public:
    explicit SettlementWidget(QWidget* parent = nullptr);

    // 打开结算页：orderId>0 直接展示；orderId<=0 时先向服务端查询当前未完成订单
    void openWithOrder(int orderId);

signals:
    void requestRecharge();
    void settled();   // 结算成功后发出，供外部刷新订单列表/充电页

protected:
    void hideEvent(QHideEvent* event) override;

private slots:
    void onSettleClicked();

private:
    void queryUnfinished();
    void queryPreview();          // ORDER_SETTLE_PREVIEW 预估本次扣费（只读）
    void updateEstimateDisplay(); // 按最新预估金额+余额刷新预估卡
    void setOrderText(const QString& text);
    void setBusy(bool busy);

    QLabel* m_orderLabel;
    QLabel* m_estimateLabel;
    QLabel* m_balanceLabel;
    QLabel* m_noteLabel;
    QPushButton* m_settleBtn;
    QPushButton* m_rechargeBtn;
    QPushButton* m_okBtn;
    QTimer* m_estTimer;
    int m_orderId = 0;
    double m_estAmount = -1.0;    // 最近一次预估金额；<0 表示尚未获取
};
