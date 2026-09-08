#include "MainWindow.h"
#include "common/NetClient.h"
#include "common/AdminSession.h"
#include "common/Theme.h"
#include "common/ApiDefs.h"
#include "common/NavRail.h"
#include "pages/MonitorWidget.h"
#include "pages/SalesWidget.h"
#include "pages/UserMgmtWidget.h"
#include "pages/StationMgmtWidget.h"
#include "pages/PileWidget.h"
#include "pages/DeviceRuntimeWidget.h"

#include <QStackedWidget>
#include <QToolBar>
#include <QLabel>
#include <QPushButton>
#include <QStatusBar>
#include <QWidget>
#include <QHBoxLayout>
#include <QVBoxLayout>
#include <QFrame>
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

    buildPages();

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
    m_statusLabel->setStyleSheet(Theme::connPillQss(connected));
}

void MainWindow::buildPages() {
    auto* central = new QWidget(this);

    // 左侧导航 + 右侧内容（页面放进栈中，构造时即创建，保持轮询/定时器运行）
    m_rail = new NavRail(central);
    m_stack = new QStackedWidget(central);

    auto* row = new QHBoxLayout(central);
    row->setContentsMargins(0, 0, 0, 0);
    row->setSpacing(0);
    row->addWidget(m_rail);

    auto* sep = new QFrame(central);
    sep->setObjectName(QStringLiteral("brandSep"));
    sep->setFixedWidth(1);
    row->addWidget(sep);

    auto* right = new QWidget(central);
    auto* rv = new QVBoxLayout(right);
    rv->setContentsMargins(22, 18, 22, 18);
    rv->setSpacing(0);
    rv->addWidget(m_stack);
    row->addWidget(right, 1);

    // 注册页面与导航项（顺序与旧页签一致）
    const auto addPage = [&](QWidget* page, const QString& label, int icon) {
        m_stack->addWidget(page);
        m_pages.append(page);
        m_rail->addItem(label, icon);
    };

    addPage(new MonitorWidget(m_stack), QStringLiteral("电桩状态监控"), 0);
    addPage(new SalesWidget(m_stack), QStringLiteral("销售业绩"), 1);
    addPage(new UserMgmtWidget(m_stack), QStringLiteral("用户管理"), 2);
    addPage(new StationMgmtWidget(m_stack), QStringLiteral("充电站管理"), 3);
    addPage(new PileWidget(m_stack), QStringLiteral("充电桩管理"), 4);
    addPage(new DeviceRuntimeWidget(m_stack), QStringLiteral("充电桩实时日志"), 5);

    connect(m_rail, &NavRail::selectionChanged, this,
            [this](int index) {
                if (index >= 0 && index < m_stack->count())
                    m_stack->setCurrentIndex(index);
            });
    m_rail->setCurrentIndex(0);
    m_stack->setCurrentIndex(0);

    setCentralWidget(central);
}

void MainWindow::onRefreshAll() {
    for (QWidget* w : m_pages) {
        if (auto* mon = qobject_cast<MonitorWidget*>(w)) mon->refresh();
        else if (auto* sale = qobject_cast<SalesWidget*>(w)) sale->refresh();
        else if (auto* usr = qobject_cast<UserMgmtWidget*>(w)) usr->refresh();
        else if (auto* st = qobject_cast<StationMgmtWidget*>(w)) st->refresh();
        else if (auto* pile = qobject_cast<PileWidget*>(w)) pile->refresh();
        else if (auto* dev = qobject_cast<DeviceRuntimeWidget*>(w)) dev->refresh();
    }
    statusBar()->showMessage(QStringLiteral("已请求刷新全部页面"), 3000);
}
