// PC 管理端（PCAdmin）程序入口
// 业务全部经 ChargingServer 处理；本端只做展示与协议请求。
// 用法：ChargingAdmin [服务器IP] [端口]   默认 127.0.0.1:9000
//       也可用环境变量 CHARGING_SERVER_HOST / CHARGING_SERVER_PORT

#include <QApplication>
#include <QEventLoop>
#include <QFont>
#include <QByteArray>

#include "common/NetClient.h"
#include "common/AdminSession.h"
#include "common/Theme.h"
#include "common/ApiDefs.h"
#include "LoginDialog.h"
#include "MainWindow.h"

int main(int argc, char* argv[]) {
    QApplication app(argc, argv);
    app.setApplicationName(QStringLiteral("ChargingAdmin"));
    app.setStyle(QStringLiteral("Fusion"));
    app.setStyleSheet(Theme::globalQss());

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

    QFont font(QStringLiteral("Noto Sans CJK SC"), 10);
    if (!font.exactMatch()) font = app.font();
    app.setFont(font);

    NetClient::instance().connectToServer(host, port);

    while (true) {
        LoginDialog login;
        if (login.exec() != QDialog::Accepted) break;

        auto* win = new MainWindow();
        win->setAttribute(Qt::WA_DeleteOnClose);
        win->show();

        bool reLogin = false;
        QObject::connect(win, &MainWindow::loggedOut, &app, [win, &reLogin]() {
            reLogin = true;
            win->close();
        });

        QEventLoop loop;
        QObject::connect(win, &QObject::destroyed, &loop, &QEventLoop::quit);
        loop.exec();

        if (!reLogin) break;
    }
    return 0;
}
