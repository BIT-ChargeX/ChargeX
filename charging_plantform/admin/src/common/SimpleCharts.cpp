#include "SimpleCharts.h"
#include "Theme.h"

#include <QPainter>
#include <QPaintEvent>
#include <QMouseEvent>
#include <QFont>
#include <QtMath>

// ================= PieChartWidget =================
PieChartWidget::PieChartWidget(QWidget* parent) : QWidget(parent) {
    setMinimumHeight(220);
    setMouseTracking(false);
}

void PieChartWidget::setData(const QVector<QPair<QString, int>>& items,
                             const QVector<QColor>& colors) {
    // 数据类别变化时重置隐藏态；否则保留用户图例开关（跨轮询刷新不丢）
    bool sameKeys = m_items.size() == items.size();
    if (sameKeys) {
        for (int i = 0; i < m_items.size(); ++i) {
            if (m_items[i].first != items[i].first) { sameKeys = false; break; }
        }
    }
    if (!sameKeys) m_hidden.clear();

    m_items = items;
    m_colors = colors;
    m_total = 0;
    for (const auto& it : items) m_total += it.second;
    update();
}

bool PieChartWidget::isHidden(int index) const {
    return m_hidden.contains(index);
}

void PieChartWidget::toggle(int index) {
    if (index < 0 || index >= m_items.size()) return;
    if (m_hidden.contains(index))
        m_hidden.remove(index);
    else
        m_hidden.insert(index);
    // 全部隐藏时回退为全显，避免图表空白
    if (m_hidden.size() >= m_items.size()) m_hidden.clear();
    update();
}

void PieChartWidget::mousePressEvent(QMouseEvent* event) {
    if (m_total <= 0) {
        QWidget::mousePressEvent(event);
        return;
    }
    const int w = width();
    const int legendW = qMax(150, w / 3);
    const int legendX = w - legendW;
    const int x = event->position().x();
    const int y = event->position().y();
    if (x >= legendX) {
        for (int i = 0; i < m_items.size(); ++i) {
            const int rowTop = 42 + i * 24;
            if (y >= rowTop - 6 && y <= rowTop + 20) {
                toggle(i);
                event->accept();
                return;
            }
        }
    }
    QWidget::mousePressEvent(event);
}

void PieChartWidget::paintEvent(QPaintEvent*) {
    QPainter p(this);
    p.setRenderHint(QPainter::Antialiasing);

    const int w = width();
    const int h = height();
    if (w <= 40 || h <= 40) return;

    // 右侧图例区
    const int legendW = qMax(150, w / 3);
    const int chartW = w - legendW - 16;
    const int side = qMax(60, qMin(chartW, h - 20));
    QRectF circle((chartW - side) / 2.0, (h - side) / 2.0, side, side);
    const int legendX = w - legendW;

    if (m_total <= 0) {
        p.setPen(Theme::textMuted());
        p.setFont(QFont(font().family(), 13));
        p.drawText(rect().adjusted(0, 0, -legendW, 0), Qt::AlignCenter,
                   QStringLiteral("暂无数据"));
        return;
    }

    // 可见分类合计（按显隐后的比例重算）
    int shownTotal = 0;
    for (int i = 0; i < m_items.size(); ++i)
        if (!isHidden(i)) shownTotal += m_items[i].second;
    if (shownTotal <= 0) shownTotal = m_total;

    // 环形占比（先整圆切分，再内挖底色形成环）
    const qreal penW = 2.0;
    p.setPen(QPen(Theme::card(), penW));
    qreal start = 90.0;   // 从正上方开始
    for (int i = 0; i < m_items.size(); ++i) {
        if (m_items[i].second <= 0 || isHidden(i)) continue;
        const qreal span = 360.0 * m_items[i].second / shownTotal;
        const QColor c = i < m_colors.size() ? m_colors[i] : Theme::textMuted();
        p.setBrush(c);
        p.drawPie(circle, static_cast<int>(start * 16), static_cast<int>(-span * 16));
        start -= span;
    }

    // 挖孔成环
    QRectF inner = circle.adjusted(circle.width() * 0.38, circle.height() * 0.38,
                                   -circle.width() * 0.38, -circle.height() * 0.38);
    p.setPen(Qt::NoPen);
    p.setBrush(Theme::card());
    p.drawEllipse(inner);

    // 右侧图例：首行 = 电桩总数（合计）
    p.setPen(Qt::NoPen);
    p.setBrush(Theme::textPrimary());
    p.drawRoundedRect(QRect(legendX + 6, 14, 12, 12), 2, 2);

    QFont boldFont = font();
    boldFont.setPixelSize(13);
    boldFont.setBold(true);
    p.setFont(boldFont);
    p.setPen(Theme::textPrimary());
    p.drawText(QRect(legendX + 24, 11, legendW - 34, 18),
               Qt::AlignLeft | Qt::AlignVCenter,
               QStringLiteral("电桩总数"));
    p.drawText(QRect(legendX + 90, 11, legendW - 96, 18),
               Qt::AlignRight | Qt::AlignVCenter, QString::number(m_total));

    // 分类图例（行可点击显隐；隐藏项置灰）
    p.setFont(font());
    int ly = 42;
    for (int i = 0; i < m_items.size(); ++i) {
        const QColor c = i < m_colors.size() ? m_colors[i] : Theme::textMuted();
        const bool hidden = isHidden(i);
        if (hidden) {
            p.setBrush(Qt::NoBrush);
            p.setPen(QPen(Theme::textMuted(), 1.2));
            p.drawRoundedRect(QRectF(legendX + 6, ly, 12, 12), 2, 2);
        } else {
            p.setPen(Qt::NoPen);
            p.setBrush(c);
            p.drawRoundedRect(QRectF(legendX + 6, ly, 12, 12), 2, 2);
        }

        const double pct = shownTotal > 0 ? m_items[i].second * 100.0 / shownTotal : 0.0;
        const QString text = QStringLiteral("%1  %2 · %3%")
                                 .arg(m_items[i].first)
                                 .arg(m_items[i].second)
                                 .arg(pct, 0, 'f', 1);
        p.setPen(hidden ? Theme::textMuted() : Theme::textSecondary());
        p.drawText(QRect(legendX + 24, ly - 3, legendW - 30, 20),
                   Qt::AlignLeft | Qt::AlignVCenter, text);
        ly += 24;
    }
}

// ================= BarChartWidget =================
BarChartWidget::BarChartWidget(QWidget* parent) : QWidget(parent) {
    setMinimumHeight(200);
    m_barColor = Theme::accent();
}

void BarChartWidget::setData(const QVector<double>& values, const QStringList& labels) {
    m_values = values;
    m_labels = labels;
    update();
}

void BarChartWidget::setBarColor(const QColor& color) {
    m_barColor = color;
    update();
}

void BarChartWidget::paintEvent(QPaintEvent*) {
    QPainter p(this);
    p.setRenderHint(QPainter::Antialiasing);

    const int w = width();
    const int h = height();
    const int n = m_values.size();
    const int left = 6, right = 6, top = 16, bottom = 26;
    const int plotW = w - left - right;
    const int plotH = h - top - bottom;
    if (n <= 0 || plotW <= 10 || plotH <= 10) {
        p.setPen(Theme::textMuted());
        p.drawText(rect(), Qt::AlignCenter, QStringLiteral("暂无数据"));
        return;
    }

    double maxVal = 1.0;
    for (double v : m_values) if (v > maxVal) maxVal = v;
    maxVal *= 1.2;

    const int nTicks = 4;
    p.setPen(Theme::border());
    for (int t = 0; t <= nTicks; ++t) {
        const int y = top + plotH - static_cast<int>(plotH * t / nTicks);
        p.drawLine(QPointF(left, y), QPointF(w - right, y));
    }

    const qreal slot = static_cast<qreal>(plotW) / n;
    const qreal barW = qMin<qreal>(slot * 0.62, 56);
    p.setFont(font());
    const int labelStep = (n > 14) ? qMax(1, n / 14) : 1;

    for (int i = 0; i < n; ++i) {
        const qreal x = left + slot * i + (slot - barW) / 2.0;
        const double v = m_values[i];
        const int barH = static_cast<int>(plotH * v / maxVal);
        const QRectF bar(x, top + plotH - barH, barW, barH);
        p.setPen(Qt::NoPen);
        p.setBrush(m_barColor);
        if (barH > 0) p.drawRoundedRect(bar, 2, 2);

        // 柱顶数值（数据点少时显示）
        if (n <= 14 && barH > 0) {
            p.setPen(Theme::textSecondary());
            p.drawText(QRect(static_cast<int>(x), static_cast<int>(bar.top()) - 16,
                             static_cast<int>(barW), 14),
                       Qt::AlignCenter, QString::number(v, 'f', 0));
        }
        // 横轴类别
        if (i % labelStep == 0 && i < m_labels.size()) {
            p.setPen(Theme::textMuted());
            p.drawText(QRect(static_cast<int>(left + slot * i), h - bottom + 4,
                             static_cast<int>(slot * labelStep), 16),
                       Qt::AlignCenter, m_labels.at(i));
        }
    }
}
