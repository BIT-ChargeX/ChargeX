#pragma once
#include <QDialog>

class QLabel;
class QDoubleSpinBox;
class QPushButton;
class RechargeRecordsWidget;

// 账户模块-需求7：余额充值（模拟支付成功 + 充值记录）
// 负责人：肇子杰   命令：USER_RECHARGE / USER_RECHARGE_RECORDS
class RechargeWidget : public QDialog {
    Q_OBJECT
public:
    explicit RechargeWidget(QWidget* parent = nullptr);

private slots:
    void onConfirmClicked();
    void onShowRecords();

private:
    void onBalanceChanged();

    QLabel* m_balanceLabel;
    QDoubleSpinBox* m_amountSpin;
    QPushButton* m_confirmBtn;
    QPushButton* m_recordsBtn;
    QLabel* m_hintLabel;

    RechargeRecordsWidget* m_recordsWidget;
};
