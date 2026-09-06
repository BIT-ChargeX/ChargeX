#pragma once
#include <QDialog>

class QTableWidget;

// 账户模块-需求7：充值记录查询（模拟支付）
// 负责人：肇子杰   命令：USER_RECHARGE_RECORDS
class RechargeRecordsWidget : public QDialog {
    Q_OBJECT
public:
    explicit RechargeRecordsWidget(QWidget* parent = nullptr);

    // 打开/刷新时调用：拉取当前用户充值记录
    void refresh();

private:
    QTableWidget* m_table;
};
