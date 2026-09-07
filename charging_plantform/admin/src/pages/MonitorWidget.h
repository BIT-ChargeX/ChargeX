#pragma once
#include <QWidget>

class QLabel;
class QTableWidget;
class QFrame;
class QTimer;
class QProgressBar;
class PieChartWidget;

// 电桩状态监控（经 PILE_MON_SUMMARY + PILE_RUNTIME_LOG_LIST）：
//   统计卡(含故障率进度)、状态分布表(含合计)、环形图(图例可显隐)、
//   阈值预警、终端实时事件流、5s 自动刷新。
class MonitorWidget : public QWidget {
    Q_OBJECT
public:
    explicit MonitorWidget(QWidget* parent = nullptr);

public slots:
    void refresh();

private:
    void loadSummary();
    void loadEvents();
    void buildPie(int inUse, int idle, int fault);

    QLabel* m_inUseValue;
    QLabel* m_idleValue;
    QLabel* m_faultValue;
    QLabel* m_refreshLabel;
    QLabel* m_alarmLabel;
    QTableWidget* m_table;
    QTableWidget* m_eventTable;
    QFrame* m_chartArea;
    QProgressBar* m_faultBar;
    QTimer* m_timer;
    PieChartWidget* m_pie = nullptr;
};
