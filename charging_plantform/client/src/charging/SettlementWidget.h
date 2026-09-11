#pragma once
#include <QDialog>

class QLabel;
class QPushButton;
<<<<<<< Updated upstream
=======
class QComboBox;
class QTimer;
class QHideEvent;
>>>>>>> Stashed changes

// 充电业务模块：未完成充电订单结算页（需求8 强制跳转目标）
// 结算/计费/扣款/释放电桩全部由服务端 ORDER_SETTLE 处理，本页只提交结算请求。
class SettlementWidget : public QDialog {
    Q_OBJECT
public:
    explicit SettlementWidget(QWidget* parent = nullptr);

    // 打开结算页：orderId>0 直接展示；orderId<=0 时先向服务端查询当前未完成订单
    void openWithOrder(int orderId);

signals:
    void requestRecharge();
    void settled();   // 结算成功后发出，供外部刷新订单列表/充电页

private slots:
    void onSettleClicked();
    void onCouponChanged(int index);

private:
    void queryUnfinished();
<<<<<<< Updated upstream
=======
    void queryPreview();          // ORDER_SETTLE_PREVIEW 预估本次扣费（只读）
    void refreshCoupons();        // 拉取可用优惠券并填充下拉框
    void updateEstimateDisplay(); // 按最新预估金额+余额刷新预估卡
>>>>>>> Stashed changes
    void setOrderText(const QString& text);
    void setBusy(bool busy);

    QLabel* m_orderLabel;
    QLabel* m_balanceLabel;
    QLabel* m_noteLabel;
    QComboBox* m_couponCombo;
    QPushButton* m_settleBtn;
    QPushButton* m_rechargeBtn;
    QPushButton* m_okBtn;
    int m_orderId = 0;
<<<<<<< Updated upstream
=======
    double m_estAmount = -1.0;    // 最近一次预估金额；<0 表示尚未获取
    double m_discount = 0.0;      // 优惠券抵扣金额
    double m_payAmount = 0.0;     // 抵扣后实付金额
    int m_selectedCouponId = 0;   // 当前选中的优惠券 redeem_id（0=不使用）
>>>>>>> Stashed changes
};
