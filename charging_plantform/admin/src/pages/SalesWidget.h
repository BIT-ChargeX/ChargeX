#pragma once
#include <QWidget>
#include <QVector>
#include <QStringList>

class QLabel;
class QComboBox;
class QDateEdit;
class QFrame;
class QTableWidget;
class BarChartWidget;

// 销售业绩（经 SALES_SUMMARY）：
//   今日/本月/总营收(带环比)、近7/30日或自定义日期区间柱状图、
//   日营收明细表、站点营收 Top5。
class SalesWidget : public QWidget {
    Q_OBJECT
public:
    explicit SalesWidget(QWidget* parent = nullptr);

public slots:
    void refresh();

private:
    void rebuildChart(const QVector<double>& values, const QStringList& labels);
    void setStatValue(QLabel* value, QLabel* delta, const QString& text,
                      double pct, const QColor& valueColor, const QString& pctLabel);

    QLabel* m_todayValue;
    QLabel* m_monthValue;
    QLabel* m_totalValue;
    QLabel* m_todayDelta;
    QLabel* m_monthDelta;
    QLabel* m_statusLabel;
    QComboBox* m_rangeCombo;
    QDateEdit* m_startEdit;
    QDateEdit* m_endEdit;
    QFrame* m_chartArea;
    QTableWidget* m_dailyTable;
    QTableWidget* m_topTable;
    BarChartWidget* m_bar = nullptr;
    bool m_loading = false;
};
