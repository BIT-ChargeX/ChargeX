// 充电用户端（Linux + Qt，模拟手机交互）程序入口
// 流程：连接服务端 -> 手机号验证码登录 -> 进入主页（找桩/充电/我的）

#include <QApplication>
#include <QStackedWidget>
#include <QFont>
#include <QByteArray>

#include "common/NetClient.h"
#include "common/AppSession.h"
#include "common/ApiDefs.h"
#include "account/LoginWidget.h"
#include "HomeWindow.h"

int main(int argc, char* argv[]) {
    QApplication app(argc, argv);
    app.setApplicationName(QStringLiteral("ChargingClient"));

    // 服务器地址解析优先级：命令行参数 > 环境变量 > ApiDefs 默认值
    // 用法：ChargingClient [host] [port]  如 ChargingClient 192.168.1.10 9000
    QString host(Api::kHost);
    quint16 port = static_cast<quint16>(Api::kPort);

    const QByteArray envHost = qgetenv("CHARGING_SERVER_HOST");
    const QByteArray envPort = qgetenv("CHARGING_SERVER_PORT");
    if (argc > 1 && argv[1][0] != '\0') {
        host = QString::fromLocal8Bit(argv[1]);
    } else if (!envHost.isEmpty()) {
        host = QString::fromLocal8Bit(envHost);
    }
    if (argc > 2) {
        bool ok = false;
        const int p = QString::fromLocal8Bit(argv[2]).toInt(&ok);
        if (ok && p > 0 && p <= 65535) port = static_cast<quint16>(p);
    } else if (!envPort.isEmpty()) {
        bool ok = false;
        const int p = QString::fromLocal8Bit(envPort).toInt(&ok);
        if (ok && p > 0 && p <= 65535) port = static_cast<quint16>(p);
    }

    NetClient::instance().connectToServer(host, port);

    QFont font(QStringLiteral("PingFang SC"), 10);
    if (!font.exactMatch()) font = app.font();
    app.setFont(font);

    auto* loginWidget = new LoginWidget;
    auto* homeWindow = new HomeWindow;

    auto* stack = new QStackedWidget;
    stack->addWidget(loginWidget);   // 0：登录
    stack->addWidget(homeWindow);    // 1：主页

    QObject::connect(loginWidget, &LoginWidget::loginSucceeded, stack,
                     [stack]() { stack->setCurrentIndex(1); });

    QObject::connect(homeWindow, &HomeWindow::logoutRequested, stack,
                     [stack]() { stack->setCurrentIndex(0); });

    // 首次登录成功：进入主页并加载数据
    QObject::connect(&AppSession::instance(), &AppSession::loginChanged, homeWindow,
                     [stack, homeWindow]() {
                         if (!AppSession::instance().isLoggedIn()) return;
                         if (stack->currentIndex() != 1) {
                             stack->setCurrentIndex(1);
                         }
                         homeWindow->onLogin();
                     });

    stack->setCurrentIndex(0);
    stack->resize(430, 760);
    stack->setWindowTitle(QStringLiteral("东软充电桩 · 用户端"));
    stack->show();

    return app.exec();
}
