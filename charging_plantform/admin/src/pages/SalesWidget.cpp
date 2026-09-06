#include "SalesWidget.h"
#include "common/NetClient.h"
#include "common/AdminSession.h"
#include "common/ApiDefs.h"
#include "common/Theme.h"
#include "common/SimpleCharts.h"

#include <QLabel>
#include <QComboBox>
#include <QFrame>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QJsonObject>
#include <QJsonArray>

namespace {
QLabel* makeStatCard(const QString& caption, QWidget* parent, QWidget** boxOut) {
    auto* box = new QFrame(parent);
    box->setObjectName(QStringLiteral("statCardSky"));
    auto* v = new QVBoxLayout(box);
    v->setContentsMargins(18, 16, 18, 16);
    v->setSpacing(4);
    auto* value = new QLabel(QStringLiteral("-"), box);
    value->setObjectName(QStringLiteral("statValue"));
    auto* cap = new QLabel(caption, box);
    cap->setObjectName(QStringLiteral("statCaption"));
    v->addWidget(value);
    v->addWidget(cap);
    if (boxOut) *boxOut = box;
    return value;
}
}

SalesWidget::SalesWidget(QWidget* parent) : QWidget(parent) {
    auto* layout = new QVBoxLayout(this);
    layout->setSpacing(12);

    QWidget* boxToday = nullptr;
    QWidget* boxMonth = nullptr;
    QWidget* boxTotal = nullptr;
    m_todayValue = makeStatCard(QStringLiteral("今日营收（元）"), this, &boxToday);
    m_monthValue = makeStatCard(QStringLiteral("本月营收（元）"), this, &boxMonth);
    m_totalValue = makeStatCard(QStringLiteral("总营收（元）"), this, &boxTotal);

    auto* statRow = new QHBoxLayout;
    statRow->addWidget(boxToday);
    statRow->addWidget(boxMonth);
    statRow->addWidget(boxTotal);
    layout->addLayout(statRow);

    auto* bar = new QHBoxLayout;
    auto* secTitle = new QLabel(QStringLiteral("营收趋势"), this);
    secTitle->setObjectName(QStringLiteral("sectionTitle"));
    bar->addWidget(secTitle);
    bar->addStretch(1);
    bar->addWidget(new QLabel(QStringLiteral("统计维度"), this));
    m_rangeCombo = new QComboBox(this);
    m_rangeCombo->addItem(QStringLiteral("近 7 日"), 7);
    m_rangeCombo->addItem(QStringLiteral("近 30 日"), 30);
    m_rangeCombo->setFixedWidth(120);
    bar->addWidget(m_rangeCombo);
    layout->addLayout(bar);

    m_chartArea = new QFrame(this);
    m_chartArea->setObjectName(QStringLiteral("card"));
    auto* cv = new QVBoxLayout(m_chartArea);
    cv->setContentsMargins(16, 12, 16, 12);
    m_bar = new BarChartWidget(m_chartArea);
    m_bar->setBarColor(Theme::accent());
    cv->addWidget(m_bar, 1);
    layout->addWidget(m_chartArea, 1);

    connect(m_rangeCombo, qOverload<int>(&QComboBox::currentIndexChanged),
            this, [this](int) { refresh(); });

    refresh();
}

void SalesWidget::rebuildChart(const QVector<double>& values, const QStringList& labels) {
    m_bar->setData(values, labels);
}

void SalesWidget::refresh() {
    QJsonObject data;
    AdminSession::instance().attach(data);
    data["days"] = m_rangeCombo->currentData().toInt();

    NetClient::instance().sendRequest(Api::CmdSalesSummary, data,
        [this](const QJsonObject& d, int code, const QString& msg) {
            if (code != 0) {
                m_todayValue->setText(QStringLiteral("失败"));
                m_totalValue->setText(msg);
                return;
            }
            m_todayValue->setText(QStringLiteral("¥%1").arg(d.value("today").toDouble(), 0, 'f', 2));
            m_monthValue->setText(QStringLiteral("¥%1").arg(d.value("month").toDouble(), 0, 'f', 2));
            m_totalValue->setText(QStringLiteral("¥%1").arg(d.value("total").toDouble(), 0, 'f', 2));

            QVector<double> values;
            QStringList labels;
            const QJsonArray daily = d.value("daily").toArray();
            for (const auto& v : daily) {
                const QJsonObject day = v.toObject();
                values.append(day.value("amount").toDouble());
                labels << day.value("date").toString();
            }
            rebuildChart(values, labels);
        });
}
