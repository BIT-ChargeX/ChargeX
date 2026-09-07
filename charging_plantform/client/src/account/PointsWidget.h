#pragma once
#include <QDialog>

class QLabel;
class QTableWidget;
class QComboBox;
class QPushButton;

// 账户模块：碳积分明细与兑换（点击"碳积分"区域进入）
// 负责人：肇子杰   命令：USER_POINTS_DETAIL / USER_POINTS_REDEEM
class PointsWidget : public QDialog {
    Q_OBJECT
public:
    explicit PointsWidget(QWidget* parent = nullptr);

    // 打开/刷新时调用：拉取积分明细与当前积分
    void refresh();

private slots:
    void onRedeemClicked();

private:
    void setBusy(bool busy);

    QLabel* m_pointsLabel;
    QTableWidget* m_table;
    QComboBox* m_itemCombo;
    QPushButton* m_redeemBtn;
    QLabel* m_hintLabel;
};
