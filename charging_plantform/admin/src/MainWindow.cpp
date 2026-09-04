#include "MainWindow.h"
#include "common/NetClient.h"
#include "common/AdminSession.h"
#include "common/Theme.h"
#include "common/ApiDefs.h"
#include "pages/MonitorWidget.h"
#include "pages/SalesWidget.h"
#include "pages/UserMgmtWidget.h"
#include "pages/StationMgmtWidget.h"
#include "pages/PileWidget.h"

#include <QTabWidget>
#include <QLabel>
#include <QPushButton>
#include <QToolBar>
#include <QStatusBar>
#include <QSizePolicy>
#include <QJsonObject>

MainWindow::MainWindow(QWidget* parent) : QMainWindow(parent) {
    setWindowTitle(QStringLiteral("充电桩运营管理 - PC管理端"));
    resize(1280, 820);

    auto* bar = new QToolBar(this);
    bar->setMovable(false);

    auto* brand = new QLabel(QStringLiteral("充电桩运营管理平台 · PC管理端"), this);
    brand->setObjectName(QStringLiteral("brandTitle"));
    bar->addWidget(brand);
    bar->addSeparator();

    m_whoLabel = new QLabel(this);
    m_whoLabel->setObjectName(QStringLiteral("userChip"));
    bar->addWidget(m_whoLabel);

    auto* spacer = new QWidget(this);
    spacer->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Preferred);
    bar->addWidget(spacer);

    m_statusLabel = new QLabel(this);
    m_statusLabel->setObjectName(QStringLiteral("statusPill"));
    bar->addWidget(m_statusLabel);

    auto* refreshBtn = new QPushButton(QStringLiteral("刷新所有"), this);
    refreshBtn->setObjectName(QStringLiteral("btnGhost"));
    auto* logoutBtn = new QPushButton(QStringLiteral("退出登录"), this);
    logoutBtn->setObjectName(QStringLiteral("btnDanger"));
    bar->addWidget(refreshBtn);
    bar->addWidget(logoutBtn);
    addToolBar(bar);

    buildTabs();
    setCentralWidget(m_tabs);

    statusBar()->showMessage(QStringLiteral("业务均通过服务器处理：ChargingServer 端口 %1").arg(Api::kPort));

    connect(refreshBtn, &QPushButton::clicked, this, &MainWindow::onRefreshAll);
    connect(logoutBtn, &QPushButton::clicked, this, [this]() {
        // 通知服务器作废会话
        QJsonObject data;
        AdminSession::instance().attach(data);
        NetClient::instance().sendRequest(Api::CmdAdminLogout, data);
        AdminSession::instance().logout();
        emit loggedOut();
    });
    connect(&NetClient::instance(), &NetClient::stateChanged, this,
            [this](int) { updateSessionUi(); });
    connect(&AdminSession::instance(), &AdminSession::loginChanged, this,
            [this]() { updateSessionUi(); });
    connect(&AdminSession::instance(), &AdminSession::loggedOut, this,
            [this]() { updateSessionUi(); });

    updateSessionUi();
    Theme::applyPointingCursor(this);
}

void MainWindow::updateSessionUi() {
    const bool connected = NetClient::instance().isConnected();
    m_whoLabel->setText(AdminSession::instance().isLoggedIn()
                            ? QStringLiteral("管理员：%1").arg(AdminSession::instance().adminName())
                            : QStringLiteral("未登录"));
    m_statusLabel->setText(connected ? QStringLiteral("已连接服务器") : QStringLiteral("服务器未连接"));
    m_statusLabel->setStyleSheet(connected
        ? QStringLiteral("color:#15803D; background:#F0FDF4; padding:3px 10px;"
                         "border-radius:10px; font-weight:600; font-size:12px;")
        : QStringLiteral("color:#B91C1C; background:#FEF2F2; padding:3px 10px;"
                         "border-radius:10px; font-weight:600; font-size:12px;"));
}

void MainWindow::buildTabs() {
    m_tabs = new QTabWidget(this);
    m_tabs->setDocumentMode(true);
    m_tabs->addTab(new MonitorWidget(m_tabs), QStringLiteral("电桩状态监控"));
    m_tabs->addTab(new SalesWidget(m_tabs), QStringLiteral("销售业绩"));
    m_tabs->addTab(new UserMgmtWidget(m_tabs), QStringLiteral("用户管理"));
    m_tabs->addTab(new StationMgmtWidget(m_tabs), QStringLiteral("充电站管理"));
    m_tabs->addTab(new PileWidget(m_tabs), QStringLiteral("充电桩管理"));
}

void MainWindow::onRefreshAll() {
    for (int i = 0; i < m_tabs->count(); ++i) {
        QWidget* w = m_tabs->widget(i);
        if (auto* mon = qobject_cast<MonitorWidget*>(w)) mon->refresh();
        else if (auto* sale = qobject_cast<SalesWidget*>(w)) sale->refresh();
        else if (auto* usr = qobject_cast<UserMgmtWidget*>(w)) usr->refresh();
        else if (auto* st = qobject_cast<StationMgmtWidget*>(w)) st->refresh();
        else if (auto* pile = qobject_cast<PileWidget*>(w)) pile->refresh();
    }
    statusBar()->showMessage(QStringLiteral("已请求刷新全部页面"), 3000);
}
