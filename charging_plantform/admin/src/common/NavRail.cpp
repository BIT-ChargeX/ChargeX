#include "NavRail.h"
#include "Theme.h"

#include <QPushButton>
#include <QButtonGroup>
#include <QVBoxLayout>
#include <QPainter>
#include <QPainterPath>
#include <QPixmap>
#include <QIcon>
#include <QColor>
#include <QPointF>

namespace {

// 程序化绘制的 20x20 线性图标（no emoji / 无外部资源）。
// kind: 0 电桩监控-闪电  1 销售-柱状  2 用户-人  3 充电站-楼  4 充电桩-插头  5 日志-列表
QIcon navIcon(int kind, const QColor& color) {
    QPixmap pm(40, 40);
    pm.setDevicePixelRatio(2.0);
    pm.fill(Qt::transparent);

    QPainter p(&pm);
    p.setRenderHint(QPainter::Antialiasing);
    QPen pen(color);
    pen.setWidthF(1.8);
    pen.setCapStyle(Qt::RoundCap);
    pen.setJoinStyle(Qt::RoundJoin);
    p.setPen(pen);
    p.setBrush(Qt::NoBrush);

    const auto L = [&p](const QPointF& a, const QPointF& b) {
        p.drawLine(a, b);
    };

    switch (kind) {
    case 0: {   // 闪电 bolt
        QPainterPath path;
        path.moveTo(12.0, 1.5);
        path.lineTo(5.2, 11.5);
        path.lineTo(9.8, 11.5);
        path.lineTo(8.2, 18.5);
        path.lineTo(15.0, 8.5);
        path.lineTo(10.4, 8.5);
        path.closeSubpath();
        p.drawPath(path);
        break;
    }
    case 1: {   // 柱状图
        L(QPointF(4, 18), QPointF(4, 10));
        L(QPointF(10, 18), QPointF(10, 4));
        L(QPointF(16, 18), QPointF(16, 13));
        p.drawLine(QPointF(2.5, 18), QPointF(17.5, 18));
        break;
    }
    case 2: {   // 用户人形
        p.drawEllipse(QRectF(7.2, 2.2, 5.6, 5.6));
        QPainterPath arc;
        arc.moveTo(3.5, 18.0);
        arc.arcTo(QRectF(3.5, 10.5, 13.0, 13.0), 180, -180);
        p.drawPath(arc);
        break;
    }
    case 3: {   // 充电站建筑
        p.drawRect(QRectF(3.2, 5.0, 13.6, 13.0));
        p.drawRect(QRectF(6.0, 2.0, 8.0, 3.0));
        p.drawRect(QRectF(5.6, 8.0, 3.4, 3.0));
        p.drawRect(QRectF(11.0, 8.0, 3.4, 3.0));
        p.drawRect(QRectF(5.6, 13.0, 3.4, 3.4));
        p.drawRect(QRectF(11.0, 13.0, 3.4, 3.4));
        break;
    }
    case 4: {   // 充电插头
        p.drawRect(QRectF(9.0, 3.5, 4.6, 6.5));
        L(QPointF(6.2, 5.2), QPointF(6.2, 2.4));
        L(QPointF(6.2, 2.4), QPointF(8.4, 2.4));
        L(QPointF(14.2, 5.2), QPointF(14.2, 2.4));
        L(QPointF(14.2, 2.4), QPointF(16.4, 2.4));
        L(QPointF(9.0, 10.0), QPointF(11.3, 10.0));
        L(QPointF(11.3, 10.0), QPointF(11.3, 14.5));
        p.drawEllipse(QRectF(10.5, 13.7, 1.6, 1.6));
        break;
    }
    case 5:     // 日志列表
    default: {
        for (int i = 0; i < 3; ++i) {
            const qreal y = 4.5 + i * 4.8;
            p.drawEllipse(QRectF(3.6, y - 1.0, 2.0, 2.0));
            L(QPointF(7.2, y), QPointF(16.4, y));
        }
        break;
    }
    }
    return QIcon(pm);
}

} // namespace

NavRail::NavRail(QWidget* parent) : QWidget(parent) {
    setObjectName(QStringLiteral("navRail"));
    setFixedWidth(216);

    auto* layout = new QVBoxLayout(this);
    layout->setContentsMargins(14, 18, 14, 18);
    layout->setSpacing(6);
    layout->addStretch(1);   // 底部占位，条目始终插在它之前 → 顶对齐

    m_group = new QButtonGroup(this);
    m_group->setExclusive(true);
}

int NavRail::count() const { return m_items.size(); }

void NavRail::addItem(const QString& label, int iconKind) {
    auto* btn = new QPushButton(label, this);
    btn->setProperty("navItem", QStringLiteral("true"));
    btn->setCheckable(true);
    btn->setIcon(navIcon(iconKind, Theme::secondary()));
    btn->setIconSize(QSize(20, 20));
    btn->setCursor(Qt::PointingHandCursor);

    m_items.append(btn);
    m_group->addButton(btn, m_items.size() - 1);
    qobject_cast<QVBoxLayout*>(layout())->insertWidget(layout()->count() - 1, btn);
    connectButton(btn, m_items.size() - 1);
}

void NavRail::connectButton(QPushButton* btn, int index) {
    connect(btn, &QPushButton::clicked, this,
            [this, btn, index]() {
                if (btn->isChecked() && index == m_current) return;
                m_current = index;
                emit selectionChanged(index);
            });
}

void NavRail::setCurrentIndex(int index) {
    if (index < 0 || index >= m_items.size()) return;
    m_current = index;
    if (auto* btn = m_items.at(index)) btn->setChecked(true);
}

int NavRail::currentIndex() const { return m_current; }
