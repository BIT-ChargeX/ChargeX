#pragma once
#include <QMainWindow>

class QTabWidget;
class QLabel;
class QPushButton;

// PC 管理端主窗口：五个业务页签，全部操作经协议请求 ChargingServer
class MainWindow : public QMainWindow {
    Q_OBJECT
public:
    explicit MainWindow(QWidget* parent = nullptr);

signals:
    void loggedOut();

private slots:
    void onRefreshAll();

private:
    void buildTabs();
    void updateSessionUi();

    QTabWidget* m_tabs;
    QLabel* m_statusLabel;
    QLabel* m_whoLabel;
};
