#pragma once
#include <QDialog>

class QLabel;
class QDoubleSpinBox;
class QPushButton;

// 账户模块-需求7：余额充值（模拟支付成功）
// 负责人：肇子杰   命令：USER_RECHARGE
class RechargeWidget : public QDialog {
    Q_OBJECT
public:
    explicit RechargeWidget(QWidget* parent = nullptr);

private slots:
    void onConfirmClicked();

private:
    void onBalanceChanged();

    QLabel* m_balanceLabel;
    QDoubleSpinBox* m_amountSpin;
    QPushButton* m_confirmBtn;
    QLabel* m_hintLabel;
};
