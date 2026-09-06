#pragma once
#include <QDialog>
#include <QJsonArray>

class QLabel;
class QTableWidget;
class QPushButton;

// 充电业务模块：我的订单列表（订单查看 + 未完成订单结算入口）
// 展示用户全部充电订单，未完成订单选中后可跳转结算页。
class OrderListWidget : public QDialog {
    Q_OBJECT
public:
    explicit OrderListWidget(QWidget* parent = nullptr);

    void refresh();   // 拉取并展示订单列表

signals:
    void settleRequested(int orderId);   // 请求结算选中的未完成订单

private slots:
    void onSettleClicked();

private:
    void updateSettleEnabled();
    void showUnfinishedBanner(int unfinishedCount);

    QLabel* m_bannerLabel;
    QTableWidget* m_table;
    QPushButton* m_settleBtn;
    QLabel* m_hintLabel;

    QJsonArray m_orders;
};
