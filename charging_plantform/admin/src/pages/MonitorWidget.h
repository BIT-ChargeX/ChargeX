#pragma once
#include <QWidget>

class QLabel;
class QTableWidget;
class QFrame;
class QTimer;
class PieChartWidget;

// 需求12：电桩状态监控（经 PILE_MON_SUMMARY 汇总 + 饼图 + 阈值预警）
class MonitorWidget : public QWidget {
    Q_OBJECT
public:
    explicit MonitorWidget(QWidget* parent = nullptr);

public slots:
    void refresh();

private:
    void buildPie(int inUse, int idle, int fault);

    QLabel* m_inUseValue;
    QLabel* m_idleValue;
    QLabel* m_faultValue;
    QLabel* m_alarmLabel;
    QTableWidget* m_table;
    QFrame* m_chartArea;
    QTimer* m_timer;
    PieChartWidget* m_pie = nullptr;
};
