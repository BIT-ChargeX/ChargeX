#include "HomeWindow.h"
#include "common/AppSession.h"
#include "account/ProfileWidget.h"
#include "account/RechargeWidget.h"
#include "station_nav/StationListWidget.h"
#include "station_nav/StationDetailWidget.h"
#include "station_nav/NavWidget.h"
#include "charging/ChargingFlowWidget.h"
#include "charging/SettlementWidget.h"

#include <QTabWidget>
#include <QVBoxLayout>
#include <QJsonObject>

namespace {
enum TabIndex { TabFindPile = 0, TabCharging = 1, TabMine = 2 };
}

HomeWindow::HomeWindow(QWidget* parent) : QWidget(parent) {
    auto* layout = new QVBoxLayout(this);
    layout->setContentsMargins(0, 0, 0, 0);

    m_tabs = new QTabWidget(this);
    m_tabs->setDocumentMode(true);

    m_stationList = new StationListWidget(this);
    m_stationDetail = new StationDetailWidget(this);
    m_nav = new NavWidget(this);
    m_charging = new ChargingFlowWidget(this);
    m_settlement = new SettlementWidget(this);
    m_recharge = new RechargeWidget(this);
    m_profile = new ProfileWidget(this);

    m_tabs->addTab(m_stationList, QStringLiteral("找桩"));
    m_tabs->addTab(m_charging, QStringLiteral("充电"));
    m_tabs->addTab(m_profile, QStringLiteral("我的"));
    layout->addWidget(m_tabs);

    // 站点卡片 -> 详情
    connect(m_stationList, &StationListWidget::stationDetailRequested,
            m_stationDetail, &StationDetailWidget::showStation);

    // 导航：列表卡片 / 站点详情
    connect(m_stationList, &StationListWidget::navRequested, this,
            [this](const QJsonObject& station) {
                m_nav->setDestination(station);
                m_nav->show();
            });
    connect(m_stationDetail, &StationDetailWidget::navRequested, this,
            [this](const QJsonObject& station) {
                m_nav->setDestination(station);
                m_nav->show();
            });

    // 选中电桩 -> 切到充电 Tab
    connect(m_stationDetail, &StationDetailWidget::pilePicked, this,
            [this](const QJsonObject& pile) {
                m_tabs->setCurrentIndex(TabCharging);
                m_charging->startChargingWithPile(pile);
            });

    // 充电页引导去"找桩"
    connect(m_charging, &ChargingFlowWidget::goPickPileRequested, this,
            [this]() { m_tabs->setCurrentIndex(TabFindPile); });

    // 结算入口：充电页快捷按钮 / 我的-我的订单列表，统一打开结算页
    connect(m_charging, &ChargingFlowWidget::settleRequested,
            m_settlement, &SettlementWidget::openWithOrder);
    connect(m_profile, &ProfileWidget::settleRequested,
            m_settlement, &SettlementWidget::openWithOrder);

    // 结算完成后刷新订单列表，并让充电页重新检测未完成订单
    connect(m_settlement, &SettlementWidget::settled, this, [this]() {
        m_profile->refreshOrders();
        m_charging->onTabEntered();
    });

    // 结算页去充值 / 我的页充值
    connect(m_settlement, &SettlementWidget::requestRecharge, this,
            [this]() { m_recharge->show(); });
    connect(m_profile, &ProfileWidget::requestRecharge, this,
            [this]() { m_recharge->show(); });

    // 退出登录
    connect(m_profile, &ProfileWidget::logoutRequested, this,
            &HomeWindow::logoutRequested);

    connect(m_tabs, &QTabWidget::currentChanged, this, &HomeWindow::onTabChanged);
}

void HomeWindow::onLogin() {
    m_tabs->setCurrentIndex(TabFindPile);
    m_stationList->refreshNearby();
    m_profile->refresh();
}

void HomeWindow::onTabChanged(int index) {
    switch (index) {
    case TabFindPile:
        m_stationList->refreshNearby();
        break;
    case TabCharging:
        if (AppSession::instance().isLoggedIn()) m_charging->onTabEntered();
        break;
    case TabMine:
        m_profile->refresh();
        break;
    default:
        break;
    }
}
