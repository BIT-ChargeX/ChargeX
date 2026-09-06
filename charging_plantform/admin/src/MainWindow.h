#pragma once
#include <QMainWindow>
#include <QList>

class QWidget;
class QLabel;
class QStackedWidget;
class NavRail;

// PC 管理端主窗口（Material 3）：顶栏 + 左侧导航栏 + 页面堆栈。
// 全部操作经协议请求 ChargingServer；refresh/轮询/登录会话逻辑保持不变。
class MainWindow : public QMainWindow {
    Q_OBJECT
public:
    explicit MainWindow(QWidget* parent = nullptr);

signals:
    void loggedOut();

private slots:
    void onRefreshAll();

private:
    void buildPages();
    void updateSessionUi();

    NavRail* m_rail;
    QStackedWidget* m_stack;
    QList<QWidget*> m_pages;
    QLabel* m_statusLabel;
    QLabel* m_whoLabel;
};
