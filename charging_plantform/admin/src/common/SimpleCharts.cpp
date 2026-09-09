#include "SimpleCharts.h"
#include "Theme.h"

#include <QPainter>
#include <QPainterPath>
#include <QPaintEvent>
#include <QMouseEvent>
#include <QFont>
#include <QDateTime>
#include <QtMath>

namespace {
constexpr qreal kPi = 3.14159265358979323846;
}

// ================= PieChartWidget =================
PieChartWidget::PieChartWidget(QWidget* parent) : QWidget(parent) {
    setMinimumHeight(220);
    setMouseTracking(false);
}

void PieChartWidget::setData(const QVector<QPair<QString, int>>& items,
                             const QVector<QColor>& colors) {
    // 数据分类变化时重置高亮；否则保留（跨轮询刷新不丢）
    bool sameKeys = m_items.size() == items.size();
    if (sameKeys) {
        for (int i = 0; i < m_items.size(); ++i) {
            if (m_items[i].first != items[i].first) { sameKeys = false; break; }
        }
    }
    if (!sameKeys) m_highlight = -1;

    m_items = items;
    m_colors = colors;
    m_total = 0;
    for (const auto& it : items) m_total += it.second;
    update();
}

void PieChartWidget::toggleHighlight(int index) {
    if (index < 0 || index >= m_items.size()) return;
    m_highlight = (m_highlight == index) ? -1 : index;   // 再点一次取消
    update();
}

// 命中检测：返回点击所在的分类下标；未命中返回 -1
int PieChartWidget::hitSlice(int x, int y) const {
    if (m_total <= 0 || m_items.isEmpty()) return -1;

    const int w = width();
    const int h = height();
    const int legendW = qMax(150, w / 3);
    const int chartW = w - legendW - 16;
    const int side = qMax(60, qMin(chartW, h - 20));
    const qreal cx = (chartW - side) / 2.0 + side / 2.0;
    const qreal cy = (h - side) / 2.0 + side / 2.0;

    const qreal dx = x - cx;
    const qreal dy = y - cy;
    const qreal r = qSqrt(dx * dx + dy * dy);
    const qreal outer = side / 2.0;
    // 内孔：沿用既有环形挖孔比例（circle.adjusted(±0.38) → 内半径 = outer*0.24）
    const qreal inner = side * 0.12;
    if (r > outer || r < inner) return -1;

    // 屏幕角(0°=3点,+y向下) 换算为“自正上方顺时针旋转”的度数
    qreal ang = qAtan2(-dy, dx) * 180.0 / kPi;
    if (ang < 0) ang += 360.0;
    qreal rot = 90.0 - ang;
    while (rot < 0.0) rot += 360.0;
    while (rot >= 360.0) rot -= 360.0;

    qreal cum = 0.0;
    for (int i = 0; i < m_items.size(); ++i) {
        const qreal span = 360.0 * m_items[i].second / m_total;
        if (span <= 0.0) continue;
        if (rot >= cum && rot < cum + span) return i;
        cum += span;
    }
    return -1;
}

void PieChartWidget::mousePressEvent(QMouseEvent* event) {
    if (m_total <= 0) {
        QWidget::mousePressEvent(event);
        return;
    }
    const int w = width();
    const int legendW = qMax(150, w / 3);
    const int legendX = w - legendW;
    const int x = static_cast<int>(event->position().x());
    const int y = static_cast<int>(event->position().y());

    // 1) 图例行
    if (x >= legendX) {
        for (int i = 0; i < m_items.size(); ++i) {
            const int rowTop = 42 + i * 24;
            if (y >= rowTop - 6 && y <= rowTop + 20) {
                toggleHighlight(i);
                event->accept();
                return;
            }
        }
    }
    // 2) 直接点扇形
    const int slice = hitSlice(x, y);
    if (slice >= 0) {
        toggleHighlight(slice);
        event->accept();
        return;
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

    // ---- 环图：三模块始终完整，比例恒按总数 ----
    // 每段以“外扇形填色 + 内孔扇形填底色”独立成环；高亮段整体外移、其余变淡
    const qreal explode = qMax(side * 0.06, 8.0);
    qreal aStart = 90.0;   // 自正上方开始、顺时针

    for (int i = 0; i < m_items.size(); ++i) {
        const qreal span = 360.0 * m_items[i].second / m_total;
        if (span <= 0.0) { aStart -= span; continue; }

        const qreal mid = aStart - span / 2.0;   // 与 drawPie 同标度
        const qreal dirX = qCos(mid * kPi / 180.0);
        const qreal dirY = -qSin(mid * kPi / 180.0);   // 屏幕 y 向下

        QRectF arc = circle;
        QColor c = i < m_colors.size() ? m_colors[i] : Theme::textMuted();
        if (m_highlight >= 0) {
            if (i == m_highlight) {
                arc.translate(dirX * explode, dirY * explode);
            } else {
                c.setAlphaF(0.35);
            }
        }

        QRectF innerArc = arc.adjusted(arc.width() * 0.38, arc.height() * 0.38,
                                       -arc.width() * 0.38, -arc.height() * 0.38);

        p.setPen(QPen(Theme::card(), 1.5));
        p.setBrush(c);
        p.drawPie(arc, static_cast<int>(aStart * 16), static_cast<int>(-span * 16));

        p.setPen(Qt::NoPen);
        p.setBrush(Theme::card());
        p.drawPie(innerArc, static_cast<int>(aStart * 16), static_cast<int>(-span * 16));

        aStart -= span;
    }

    // ---- 图例：首行 = 电桩总数（合计） ----
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

    // 分类图例：高亮行实色+加粗深字；非高亮行稍淡
    int ly = 42;
    for (int i = 0; i < m_items.size(); ++i) {
        const bool lit = (m_highlight == i);
        QColor c = i < m_colors.size() ? m_colors[i] : Theme::textMuted();
        if (m_highlight >= 0 && !lit) c.setAlphaF(0.45);

        p.setPen(Qt::NoPen);
        p.setBrush(c);
        p.drawRoundedRect(QRectF(legendX + 6, ly, 12, 12), 2, 2);

        QFont rowFont = font();
        if (lit) {
            rowFont.setBold(true);
            rowFont.setPixelSize(13);
        }
        p.setFont(rowFont);
        const double pct = m_total > 0 ? m_items[i].second * 100.0 / m_total : 0.0;
        const QString text = QStringLiteral("%1  %2 · %3%")
                                 .arg(m_items[i].first)
                                 .arg(m_items[i].second)
                                 .arg(pct, 0, 'f', 1);
        p.setPen(lit ? Theme::textPrimary() : Theme::textSecondary());
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

// ================= LineChartWidget =================
LineChartWidget::LineChartWidget(QWidget* parent) : QWidget(parent) {
    setMinimumHeight(180);
}

void LineChartWidget::setSeries(const QVector<qint64>& tsMs,
                                const QVector<double>& power) {
    m_ts = tsMs;
    m_power = power;
    update();
}

void LineChartWidget::setWindow(qint64 startMs, qint64 endMs) {
    m_reqStart = startMs;
    m_reqEnd = endMs;
    update();
}

void LineChartWidget::setHint(const QString& text) {
    m_hint = text;
    update();
}

void LineChartWidget::paintEvent(QPaintEvent*) {
    QPainter p(this);
    p.setRenderHint(QPainter::Antialiasing);

    const int w = width();
    const int h = height();
    if (w <= 60 || h <= 50) return;

    const int left = 46, right = 14, top = 16, bottom = 26;
    const int plotW = w - left - right;
    const int plotH = h - top - bottom;
    if (plotW <= 10 || plotH <= 10) return;

    const bool hasReq = m_reqStart >= 0 && m_reqEnd > m_reqStart;
    const bool hasData = !m_power.isEmpty() && m_ts.size() == m_power.size();

    const auto emptyText = [&]() -> QString {
        if (!m_hint.isEmpty()) return m_hint;
        if (!hasReq) return QStringLiteral("暂无遥测数据");
        return QStringLiteral("该时间窗暂无样本（桩未在用或未上报）");
    };

    // 无请求窗口也无数据：仅提示
    if (!hasReq && !hasData) {
        p.setPen(Theme::textMuted());
        p.drawText(rect(), Qt::AlignCenter, emptyText());
        return;
    }

    // 时间轴范围：优先“请求窗口（真实时间）”，否则用数据起止
    const qint64 t0 = hasReq ? m_reqStart
                             : (hasData ? m_ts.first() : QDateTime::currentMSecsSinceEpoch());
    const qint64 t1 = hasReq ? m_reqEnd
                             : (hasData ? m_ts.last() : t0);
    const double spanMs = qMax<qint64>(1, t1 - t0);

    // Y 轴范围：有数据按数据放大，否则 0..1（仅显示时间轴）
    double maxP = 1.0;
    if (hasData) {
        for (double v : m_power) if (v > maxP) maxP = v;
    }
    const double yMax = maxP * 1.15;

    const QRectF plot(left, top, plotW, plotH);
    const auto mapX = [&](qint64 t) {
        return left + plotW * (t - t0) / spanMs;
    };
    const auto mapY = [&](double v) {
        return top + plotH - plotH * (v / yMax);
    };

    // 网格 + Y 轴刻度
    p.setFont(font());
    const int nYTicks = 4;
    for (int t = 0; t <= nYTicks; ++t) {
        const double v = yMax * t / nYTicks;
        const qreal y = mapY(v);
        p.setPen(Theme::border());
        p.drawLine(QPointF(left, y), QPointF(left + plotW, y));
        p.setPen(Theme::textMuted());
        p.drawText(QRectF(0, y - 7, left - 8, 14),
                   Qt::AlignRight | Qt::AlignVCenter, QString::number(v, 'f', 1));
    }

    // X 轴时间刻度（按真实窗口绘制，时间轴与墙上时间对应）
    const bool needSec = spanMs < 3600LL * 1000;
    const QString fmt = needSec ? QStringLiteral("HH:mm:ss") : QStringLiteral("HH:mm");
    const int nXTicks = 5;
    for (int t = 0; t < nXTicks; ++t) {
        const qint64 ts = t0 + static_cast<qint64>(spanMs * t / (nXTicks - 1));
        const qreal x = mapX(ts);
        p.setPen(Theme::border());
        p.drawLine(QPointF(x, top), QPointF(x, top + plotH));
        p.setPen(Theme::textMuted());
        const QString label = QDateTime::fromMSecsSinceEpoch(ts)
                                  .toLocalTime()
                                  .toString(fmt);
        p.drawText(QRectF(x - 45, top + plotH + 6, 90, 16),
                   Qt::AlignHCenter | Qt::AlignTop, label);
    }

    if (!hasData) {
        p.setPen(Theme::textMuted());
        p.drawText(plot, Qt::AlignCenter, emptyText());
        return;
    }

    // 折线
    QPainterPath path;
    for (int i = 0; i < m_power.size(); ++i) {
        const QPointF pt(mapX(m_ts[i]), mapY(m_power[i]));
        if (i == 0) path.moveTo(pt);
        else path.lineTo(pt);
    }
    p.setPen(QPen(Theme::accent(), 2.0));
    p.setBrush(Qt::NoBrush);
    p.drawPath(path);

    // 数据点（量少才画）
    if (m_power.size() <= 240) {
        p.setBrush(Theme::accent());
        p.setPen(Qt::NoPen);
        for (int i = 0; i < m_power.size(); ++i) {
            p.drawEllipse(QPointF(mapX(m_ts[i]), mapY(m_power[i])), 2.2, 2.2);
        }
    }
}
