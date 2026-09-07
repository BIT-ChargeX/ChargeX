#pragma once
#include <QWidget>
#include <QVector>
#include <QPair>
#include <QColor>
#include <QStringList>

class QMouseEvent;

// 依赖 Qt Widgets 的轻量图表（QPainter 自绘，不依赖 QtCharts 可选模块）。
// PieChartWidget：环形状态分布图 + 右侧图例。
// 三个模块始终完整展示；点击图例行或扇形可“高亮”该分类（扇区外弹 + 其余变淡），
// 再次点击同一分类恢复三块等权展示（不隐藏、不重算比例）。
class PieChartWidget : public QWidget {
    Q_OBJECT
public:
    explicit PieChartWidget(QWidget* parent = nullptr);

    void setData(const QVector<QPair<QString, int>>& items,
                 const QVector<QColor>& colors);

protected:
    void paintEvent(QPaintEvent* event) override;
    void mousePressEvent(QMouseEvent* event) override;

private:
    void toggleHighlight(int index);
    int hitSlice(int x, int y) const;

    QVector<QPair<QString, int>> m_items;
    QVector<QColor> m_colors;
    int m_total = 0;
    int m_highlight = -1;   // -1 = 不高亮；否则为高亮分类下标
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
    QColor m_barColor;
};
