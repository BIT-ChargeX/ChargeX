#pragma once
#include <QWidget>
#include <QVector>
#include <QStringList>

class QLabel;
class QComboBox;
class QFrame;
class BarChartWidget;

// 销售业绩（经 SALES_SUMMARY）：今日/本月/总营收 + 近7/30日柱状图
class SalesWidget : public QWidget {
    Q_OBJECT
public:
    explicit SalesWidget(QWidget* parent = nullptr);

public slots:
    void refresh();

private:
    void rebuildChart(const QVector<double>& values, const QStringList& labels);

    QLabel* m_todayValue;
    QLabel* m_monthValue;
    QLabel* m_totalValue;
    QComboBox* m_rangeCombo;
    QFrame* m_chartArea;
    BarChartWidget* m_bar = nullptr;
};
