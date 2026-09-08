#pragma once
#include <QWidget>
#include <QVector>
#include <QPair>
#include <QColor>
#include <QStringList>

class QMouseEvent;

// 依赖 Qt Widgets 的轻量图表（QPainter 自绘，不依赖 QtCharts 可选模块）。
// PieChartWidget：环形状态分布图 + 右侧图例；点击图例行/扇形可高亮分类（外弹+其余变淡），
// 再点取消；三模块始终完整展示，不隐藏不重算。
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

// LineChartWidget：时间轴功率折线图（X=时间 ms，Y=功率 kW）
class LineChartWidget : public QWidget {
    Q_OBJECT
public:
    explicit LineChartWidget(QWidget* parent = nullptr);

    // 传入按时间升序的点；传空即显示“暂无遥测数据”
    void setSeries(const QVector<qint64>& tsMs, const QVector<double>& power);

protected:
    void paintEvent(QPaintEvent* event) override;

private:
    QVector<qint64> m_ts;
    QVector<double> m_power;
};
