#pragma once
#include <QWidget>
#include <QVector>
#include <QPair>
#include <QColor>
#include <QStringList>

// 依赖 Qt Widgets 的轻量图表（QPainter 自绘，不依赖 QtCharts 可选模块）。
// PieChartWidget：环形状态分布图 + 右侧图例
class PieChartWidget : public QWidget {
    Q_OBJECT
public:
    explicit PieChartWidget(QWidget* parent = nullptr);

    void setData(const QVector<QPair<QString, int>>& items,
                 const QVector<QColor>& colors);

protected:
    void paintEvent(QPaintEvent* event) override;

private:
    QVector<QPair<QString, int>> m_items;
    QVector<QColor> m_colors;
    int m_total = 0;
};

// BarChartWidget：竖向柱状图 + 刻度线 + 数值/类别标签
class BarChartWidget : public QWidget {
    Q_OBJECT
public:
    explicit BarChartWidget(QWidget* parent = nullptr);

    void setData(const QVector<double>& values, const QStringList& labels);
    void setBarColor(const QColor& color);

protected:
    void paintEvent(QPaintEvent* event) override;

private:
    QVector<double> m_values;
    QStringList m_labels;
    QColor m_barColor = QColor("#0369A1");
};
