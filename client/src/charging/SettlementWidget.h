#pragma once
#include <QDialog>

class QLabel;
class QPushButton;

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

private slots:
    void onSettleClicked();

private:
    void queryUnfinished();
    void setOrderText(const QString& text);
    void setBusy(bool busy);

    QLabel* m_orderLabel;
    QLabel* m_balanceLabel;
    QLabel* m_noteLabel;
    QPushButton* m_settleBtn;
    QPushButton* m_rechargeBtn;
    QPushButton* m_okBtn;
    int m_orderId = 0;
};
