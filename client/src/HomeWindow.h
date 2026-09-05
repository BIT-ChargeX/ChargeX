#pragma once
#include <QWidget>

class QTabWidget;
class StationListWidget;
class StationDetailWidget;
class NavWidget;
class ChargingFlowWidget;
class SettlementWidget;
class RechargeWidget;
class ProfileWidget;

// 登录后主页：三个 Tab（找桩 / 充电 / 我的）+ 各模块子页面装配
class HomeWindow : public QWidget {
    Q_OBJECT
public:
    explicit HomeWindow(QWidget* parent = nullptr);

public slots:
    void onLogin();

signals:
    void logoutRequested();

private slots:
    void onTabChanged(int index);

private:
    QTabWidget* m_tabs;
    StationListWidget* m_stationList;
    StationDetailWidget* m_stationDetail;
    NavWidget* m_nav;
    ChargingFlowWidget* m_charging;
    SettlementWidget* m_settlement;
    RechargeWidget* m_recharge;
    ProfileWidget* m_profile;
};
