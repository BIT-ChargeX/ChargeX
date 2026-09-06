#include "SalesWidget.h"
#include "common/NetClient.h"
#include "common/AdminSession.h"
#include "common/ApiDefs.h"
#include "common/Theme.h"
#include "common/SimpleCharts.h"

#include <QLabel>
#include <QComboBox>
#include <QDateEdit>
#include <QFrame>
#include <QTableWidget>
#include <QTableWidgetItem>
#include <QHeaderView>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QBrush>
#include <QAbstractItemView>
#include <QDate>
#include <QJsonObject>
#include <QJsonArray>

namespace {

QString signedPct(double pct) {
    if (pct < -0.5) return QStringLiteral("-");   // -1 表示无基线
    const QString s = QString::number(pct, 'f', 1);
    if (pct > 0.0) return QStringLiteral("+%1%").arg(s);
    return QStringLiteral("%1%").arg(s);
}

QLabel* makeValueLabel(const QString& text, QWidget* parent) {
    auto* lb = new QLabel(text, parent);
    lb->setObjectName(QStringLiteral("statValue"));
    return lb;
}

QLabel* makeCaptionLabel(const QString& text, QWidget* parent) {
    auto* lb = new QLabel(text, parent);
    lb->setObjectName(QStringLiteral("statCaption"));
    return lb;
}

QLabel* makeDeltaLabel(QWidget* parent) {
    auto* lb = new QLabel(QStringLiteral("-"), parent);
    lb->setObjectName(QStringLiteral("statDelta"));
    lb->setStyleSheet(QStringLiteral("color:%1; font-size:12px; font-weight:600;")
                          .arg(Theme::textMuted().name()));
    return lb;
}

QFrame* makeCardFrame(QWidget* parent) {
    auto* f = new QFrame(parent);
    f->setObjectName(QStringLiteral("card"));
    return f;
}

} // namespace

SalesWidget::SalesWidget(QWidget* parent) : QWidget(parent) {
    auto* layout = new QVBoxLayout(this);
    layout->setSpacing(12);

    // 统计卡（今日 / 本月 / 累计 + 环比）
    auto makeStat = [](const QString& cap, QWidget* parent,
                       QLabel** vOut, QLabel** dOut) {
        auto* box = new QFrame(parent);
        box->setObjectName(QStringLiteral("statCardSky"));
        auto* v = new QVBoxLayout(box);
        v->setContentsMargins(18, 16, 18, 16);
        v->setSpacing(4);
        auto* val = makeValueLabel(QStringLiteral("-"), box);
        auto* caption = makeCaptionLabel(cap, box);
        auto* delta = makeDeltaLabel(box);
        v->addWidget(val);
        v->addWidget(caption);
        v->addSpacing(2);
        v->addWidget(delta);
        if (vOut) *vOut = val;
        if (dOut) *dOut = delta;
        return box;
    };

    QWidget* b1 = nullptr;
    QWidget* b2 = nullptr;
    QWidget* b3 = nullptr;
    b1 = makeStat(QStringLiteral("今日营收（元）"), this, &m_todayValue, &m_todayDelta);
    b2 = makeStat(QStringLiteral("本月营收（元）"), this, &m_monthValue, &m_monthDelta);
    b3 = makeStat(QStringLiteral("总营收（元）"), this, &m_totalValue, nullptr);
    auto* statRow = new QHBoxLayout;
    statRow->addWidget(b1);
    statRow->addWidget(b2);
    statRow->addWidget(b3);
    layout->addLayout(statRow);

    // 图区头部：维度切换 + 自定义日期
    auto* bar = new QHBoxLayout;
    auto* secTitle = new QLabel(QStringLiteral("营收趋势"), this);
    secTitle->setObjectName(QStringLiteral("sectionTitle"));
    bar->addWidget(secTitle);
    bar->addStretch(1);

    auto* dimLabel = new QLabel(QStringLiteral("统计维度"), this);
    dimLabel->setObjectName(QStringLiteral("cardCaption"));
    bar->addWidget(dimLabel);

    m_rangeCombo = new QComboBox(this);
    m_rangeCombo->addItem(QStringLiteral("近 7 日"), 7);
    m_rangeCombo->addItem(QStringLiteral("近 30 日"), 30);
    m_rangeCombo->addItem(QStringLiteral("自定义"), -1);
    m_rangeCombo->setFixedWidth(120);
    bar->addWidget(m_rangeCombo);

    m_startEdit = new QDateEdit(this);
    m_startEdit->setCalendarPopup(true);
    m_startEdit->setDisplayFormat(QStringLiteral("yyyy-MM-dd"));
    m_startEdit->setDate(QDate::currentDate().addDays(-6));
    m_startEdit->setFixedWidth(118);
    m_endEdit = new QDateEdit(this);
    m_endEdit->setCalendarPopup(true);
    m_endEdit->setDisplayFormat(QStringLiteral("yyyy-MM-dd"));
    m_endEdit->setDate(QDate::currentDate());
    m_endEdit->setFixedWidth(118);
    bar->addWidget(m_startEdit);
    auto* sepLabel = new QLabel(QStringLiteral("至"), this);
    sepLabel->setObjectName(QStringLiteral("cardCaption"));
    bar->addWidget(sepLabel);
    bar->addWidget(m_endEdit);
    layout->addLayout(bar);

    m_statusLabel = new QLabel(this);
    m_statusLabel->setObjectName(QStringLiteral("hintMuted"));
    layout->addWidget(m_statusLabel);

    // 营收趋势柱状图
    m_chartArea = makeCardFrame(this);
    auto* cv = new QVBoxLayout(m_chartArea);
    cv->setContentsMargins(16, 12, 16, 12);
    m_bar = new BarChartWidget(m_chartArea);
    m_bar->setBarColor(Theme::accent());
    cv->addWidget(m_bar, 1);
    layout->addWidget(m_chartArea, 1);

    // 明细 + Top5
    auto* detailRow = new QHBoxLayout;
    detailRow->setSpacing(12);

    auto* dailyCard = makeCardFrame(this);
    auto* dv = new QVBoxLayout(dailyCard);
    dv->setContentsMargins(16, 12, 16, 12);
    dv->setSpacing(8);
    auto* dailyTitle = new QLabel(QStringLiteral("日营收明细"), dailyCard);
    dailyTitle->setObjectName(QStringLiteral("sectionTitle"));
    dv->addWidget(dailyTitle);
    m_dailyTable = new QTableWidget(dailyCard);
    m_dailyTable->setColumnCount(2);
    m_dailyTable->setHorizontalHeaderLabels({QStringLiteral("日期"), QStringLiteral("营收(元)")});
    m_dailyTable->horizontalHeader()->setSectionResizeMode(QHeaderView::Stretch);
    m_dailyTable->setEditTriggers(QAbstractItemView::NoEditTriggers);
    m_dailyTable->setSelectionBehavior(QAbstractItemView::SelectRows);
    m_dailyTable->setAlternatingRowColors(true);
    m_dailyTable->verticalHeader()->setVisible(false);
    m_dailyTable->setMinimumHeight(120);
    dv->addWidget(m_dailyTable);

    auto* topCard = makeCardFrame(this);
    auto* tv = new QVBoxLayout(topCard);
    tv->setContentsMargins(16, 12, 16, 12);
    tv->setSpacing(8);
    auto* topTitle = new QLabel(QStringLiteral("站点营收 Top5"), topCard);
    topTitle->setObjectName(QStringLiteral("sectionTitle"));
    tv->addWidget(topTitle);
    m_topTable = new QTableWidget(topCard);
    m_topTable->setColumnCount(2);
    m_topTable->setHorizontalHeaderLabels({QStringLiteral("充电站"), QStringLiteral("营收(元)")});
    m_topTable->horizontalHeader()->setSectionResizeMode(QHeaderView::Stretch);
    m_topTable->setEditTriggers(QAbstractItemView::NoEditTriggers);
    m_topTable->setSelectionBehavior(QAbstractItemView::SelectRows);
    m_topTable->setAlternatingRowColors(true);
    m_topTable->verticalHeader()->setVisible(false);
    m_topTable->setMinimumHeight(120);
    tv->addWidget(m_topTable);

    detailRow->addWidget(dailyCard, 1);
    detailRow->addWidget(topCard, 1);
    layout->addLayout(detailRow);

    // 交互
    m_startEdit->hide();
    m_endEdit->hide();
    sepLabel->hide();

    auto applyCustomVisibility = [this, sepLabel]() {
        const bool custom = m_rangeCombo->currentData().toInt() == -1;
        m_startEdit->setVisible(custom);
        m_endEdit->setVisible(custom);
        sepLabel->setVisible(custom);
    };

    connect(m_rangeCombo, qOverload<int>(&QComboBox::currentIndexChanged),
            this, [this, applyCustomVisibility](int) {
                applyCustomVisibility();
                if (m_rangeCombo->currentData().toInt() == -1) {
                    const QDate end = QDate::currentDate();
                    m_startEdit->setDate(end.addDays(-6));
                    m_endEdit->setDate(end);
                }
                refresh();
            });
    connect(m_startEdit, &QDateEdit::dateChanged, this,
            [this](const QDate&) {
                if (m_rangeCombo->currentData().toInt() == -1) refresh();
            });
    connect(m_endEdit, &QDateEdit::dateChanged, this,
            [this](const QDate&) {
                if (m_rangeCombo->currentData().toInt() == -1) refresh();
            });

    applyCustomVisibility();
    refresh();
}

void SalesWidget::setStatValue(QLabel* value, QLabel* delta, const QString& text,
                               double pct, const QColor& valueColor,
                               const QString& pctLabel) {
    value->setText(text);
    value->setStyleSheet(QStringLiteral("color:%1;").arg(valueColor.name()));
    if (!delta) return;
    if (pct < -0.5) {
        delta->setText(QStringLiteral("-"));
        delta->setStyleSheet(QStringLiteral("color:%1; font-size:12px; font-weight:600;")
                                 .arg(Theme::textMuted().name()));
        return;
    }
    const QColor c = pct > 0.0 ? Theme::success()
                     : pct < 0.0 ? Theme::danger()
                                 : Theme::textMuted();
    delta->setText(QStringLiteral("%1 %2").arg(pctLabel, signedPct(pct)));
    delta->setStyleSheet(QStringLiteral("color:%1; font-size:12px; font-weight:600;")
                             .arg(c.name()));
}

void SalesWidget::refresh() {
    if (m_loading) return;
    m_loading = true;
    m_rangeCombo->setEnabled(false);
    m_startEdit->setEnabled(false);
    m_endEdit->setEnabled(false);

    QJsonObject data;
    AdminSession::instance().attach(data);
    if (m_rangeCombo->currentData().toInt() == -1) {
        const QDate start = m_startEdit->date();
        const QDate end = m_endEdit->date();
        if (start > end) {
            m_statusLabel->setText(QStringLiteral("开始日期不能晚于结束日期"));
            m_loading = false;
            m_rangeCombo->setEnabled(true);
            m_startEdit->setEnabled(true);
            m_endEdit->setEnabled(true);
            return;
        }
        data["start"] = start.toString(Qt::ISODate);
        data["end"] = end.toString(Qt::ISODate);
    } else {
        data["days"] = m_rangeCombo->currentData().toInt();
    }

    NetClient::instance().sendRequest(Api::CmdSalesSummary, data,
        [this](const QJsonObject& d, int code, const QString& msg) {
            m_loading = false;
            m_rangeCombo->setEnabled(true);
            const bool custom = m_rangeCombo->currentData().toInt() == -1;
            m_startEdit->setEnabled(custom);
            m_endEdit->setEnabled(custom);

            if (code != 0) {
                m_todayValue->setText(QStringLiteral("失败"));
                m_totalValue->setText(QStringLiteral("-"));
                m_statusLabel->setText(QStringLiteral("加载失败：%1").arg(msg));
                return;
            }

            setStatValue(m_todayValue, m_todayDelta,
                         QStringLiteral("¥%1").arg(d.value("today").toDouble(), 0, 'f', 2),
                         d.value("today_pct").toDouble(-1.0), Theme::primary(),
                         QStringLiteral("较昨日"));
            setStatValue(m_monthValue, m_monthDelta,
                         QStringLiteral("¥%1").arg(d.value("month").toDouble(), 0, 'f', 2),
                         d.value("month_pct").toDouble(-1.0), Theme::secondary(),
                         QStringLiteral("较上月同期"));
            setStatValue(m_totalValue, nullptr,
                         QStringLiteral("¥%1").arg(d.value("total").toDouble(), 0, 'f', 2),
                         0.0, Theme::textPrimary(), QString());

            m_statusLabel->setText(
                QStringLiteral("区间 %1 ~ %2 · 共 %3 天")
                    .arg(d.value("start").toString())
                    .arg(d.value("end").toString())
                    .arg(d.value("days").toInt()));

            QVector<double> values;
            QStringList labels;
            const QJsonArray daily = d.value("daily").toArray();

            m_dailyTable->setRowCount(daily.size());
            for (int r = 0; r < daily.size(); ++r) {
                const QJsonObject day = daily.at(r).toObject();
                values.append(day.value("amount").toDouble());
                labels << day.value("date").toString();
                auto* dIt = new QTableWidgetItem(day.value("date_full").toString());
                dIt->setTextAlignment(Qt::AlignCenter);
                auto* aIt = new QTableWidgetItem(
                    QStringLiteral("¥%1").arg(day.value("amount").toDouble(), 0, 'f', 2));
                aIt->setTextAlignment(Qt::AlignCenter);
                m_dailyTable->setItem(r, 0, dIt);
                m_dailyTable->setItem(r, 1, aIt);
            }
            rebuildChart(values, labels);

            const QJsonArray top = d.value("top_stations").toArray();
            m_topTable->setRowCount(top.isEmpty() ? 1 : top.size());
            if (top.isEmpty()) {
                auto* empty = new QTableWidgetItem(QStringLiteral("本期暂无站点营收"));
                empty->setTextAlignment(Qt::AlignCenter);
                empty->setForeground(QBrush(Theme::textMuted()));
                m_topTable->setItem(0, 0, empty);
                m_topTable->setSpan(0, 0, 1, 2);
            } else {
                for (int r = 0; r < top.size(); ++r) {
                    const QJsonObject st = top.at(r).toObject();
                    auto* nIt = new QTableWidgetItem(
                        QStringLiteral("%1. %2").arg(r + 1).arg(st.value("name").toString()));
                    nIt->setTextAlignment(Qt::AlignCenter);
                    auto* aIt = new QTableWidgetItem(
                        QStringLiteral("¥%1").arg(st.value("amount").toDouble(), 0, 'f', 2));
                    aIt->setTextAlignment(Qt::AlignCenter);
                    m_topTable->setItem(r, 0, nIt);
                    m_topTable->setItem(r, 1, aIt);
                }
            }
        });
}

void SalesWidget::rebuildChart(const QVector<double>& values, const QStringList& labels) {
    m_bar->setData(values, labels);
}
