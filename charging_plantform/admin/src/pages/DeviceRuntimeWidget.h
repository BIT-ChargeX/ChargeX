#pragma once
#include <QWidget>

class QTableWidget;
class QLabel;
class QTimer;

// 充电桩实时运行日志（经 PILE_RUNTIME_LOG_LIST 读取）
// 展示充电桩终端的上线/状态变化/控制指令回执事件；顶部显示在线桩数。
class DeviceRuntimeWidget : public QWidget {
    Q_OBJECT
public:
    explicit DeviceRuntimeWidget(QWidget* parent = nullptr);

    void refresh();

private:
    void loadLogs();

    QTableWidget* m_table;
    QLabel* m_statusLabel;
    QTimer* m_timer;
};
