#include "MonitorWidget.h"
#include "common/NetClient.h"
#include "common/AdminSession.h"
#include "common/ApiDefs.h"
#include "common/Theme.h"
#include "common/SimpleCharts.h"

#include <QLabel>
#include <QTableWidget>
#include <QTableWidgetItem>
#include <QHeaderView>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QSplitter>
#include <QFrame>
#include <QTimer>
#include <QList>
#include <QPair>
#include <QColor>
#include <QJsonObject>

namespace {
QLabel* makeStatCard(const QString& caption, const QString& accentName,
                     QWidget* parent, QWidget** boxOut) {
    auto* box = new QFrame(parent);
    box->setObjectName(QStringLiteral("statCard%1").arg(accentName));
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

MonitorWidget::MonitorWidget(QWidget* parent) : QWidget(parent) {
    auto* layout = new QVBoxLayout(this);
    layout->setSpacing(12);

    QWidget* boxInUse = nullptr;
    QWidget* boxIdle = nullptr;
    QWidget* boxFault = nullptr;
    m_inUseValue = makeStatCard(QStringLiteral("在用（含预约）"), QStringLiteral("Sky"), this, &boxInUse);
    m_idleValue = makeStatCard(QStringLiteral("闲置"), QStringLiteral("Green"), this, &boxIdle);
    m_faultValue = makeStatCard(QStringLiteral("故障"), QStringLiteral("Red"), this, &boxFault);

    auto* statRow = new QHBoxLayout;
    statRow->addWidget(boxInUse);
    statRow->addWidget(boxIdle);
    statRow->addWidget(boxFault);
    layout->addLayout(statRow);

    auto* splitter = new QSplitter(this);
    auto* left = new QWidget(splitter);
    auto* lv = new QVBoxLayout(left);
    auto* leftTitle = new QLabel(QStringLiteral("状态分布明细"), left);
    leftTitle->setObjectName(QStringLiteral("sectionTitle"));
    lv->addWidget(leftTitle);
    m_table = new QTableWidget(left);
    m_table->setColumnCount(3);
    m_table->setHorizontalHeaderLabels({QStringLiteral("状态"), QStringLiteral("数量"), QStringLiteral("占比")});
    m_table->horizontalHeader()->setSectionResizeMode(QHeaderView::Stretch);
    m_table->setSelectionBehavior(QAbstractItemView::SelectRows);
    m_table->setEditTriggers(QAbstractItemView::NoEditTriggers);
    m_table->setAlternatingRowColors(true);
    m_table->verticalHeader()->setVisible(false);
    lv->addWidget(m_table, 1);
    splitter->addWidget(left);

    m_chartArea = new QFrame(splitter);
    m_chartArea->setObjectName(QStringLiteral("card"));
    auto* cv = new QVBoxLayout(m_chartArea);
    cv->setContentsMargins(16, 12, 16, 12);
    auto* chartTitle = new QLabel(QStringLiteral("状态分布（饼图）"), m_chartArea);
    chartTitle->setObjectName(QStringLiteral("sectionTitle"));
    cv->addWidget(chartTitle);
    m_pie = new PieChartWidget(m_chartArea);
    cv->addWidget(m_pie, 1);
    splitter->addWidget(m_chartArea);
    splitter->setStretchFactor(0, 1);
    splitter->setStretchFactor(1, 1);
    layout->addWidget(splitter, 1);

    m_alarmLabel = new QLabel(this);
    m_alarmLabel->setObjectName(QStringLiteral("alarmBox"));
    layout->addWidget(m_alarmLabel);

    m_timer = new QTimer(this);
    m_timer->setInterval(5000);
    connect(m_timer, &QTimer::timeout, this, &MonitorWidget::refresh);
    m_timer->start();

    refresh();
}

void MonitorWidget::buildPie(int inUse, int idle, int fault) {
    QVector<QPair<QString, int>> parts = {
        {QStringLiteral("在用"), inUse},
        {QStringLiteral("闲置"), idle},
        {QStringLiteral("故障"), fault},
    };
    QVector<QColor> colors = {
        Theme::statusText(QStringLiteral("在用")),
        Theme::statusText(QStringLiteral("闲置")),
        Theme::statusText(QStringLiteral("故障")),
    };
    m_pie->setData(parts, colors);
}

void MonitorWidget::refresh() {
    QJsonObject data;
    AdminSession::instance().attach(data);

    NetClient::instance().sendRequest(Api::CmdPileMonSummary, data,
        [this](const QJsonObject& d, int code, const QString& msg) {
            if (code != 0) {
                m_alarmLabel->setText(QStringLiteral("加载失败：%1").arg(msg));
                m_alarmLabel->setStyleSheet(Theme::alarmQss(false));
                return;
            }
            const int total = d.value("total").toInt();
            const int inUse = d.value("in_use").toInt();
            const int idle = d.value("idle").toInt();
            const int fault = d.value("fault").toInt();
            const double ratio = d.value("fault_ratio").toDouble();

            m_inUseValue->setText(QString::number(inUse));
            m_idleValue->setText(QString::number(idle));
            m_faultValue->setText(QString::number(fault));

            struct Row { QString name; int count; };
            QList<Row> rows = {
                {QStringLiteral("在用（含预约）"), inUse},
                {QStringLiteral("闲置"), idle},
                {QStringLiteral("故障"), fault},
            };
            m_table->setRowCount(rows.size());
            for (int i = 0; i < rows.size(); ++i) {
                double pct = total > 0 ? rows[i].count * 100.0 / total : 0.0;
                QStringList cols = {rows[i].name, QString::number(rows[i].count),
                                    QStringLiteral("%1%").arg(pct, 0, 'f', 1)};
                for (int c = 0; c < cols.size(); ++c) {
                    auto* it = new QTableWidgetItem(cols.at(c));
                    it->setTextAlignment(Qt::AlignCenter);
                    m_table->setItem(i, c, it);
                }
                m_table->item(i, 0)->setForeground(Theme::textSecondary());
            }
            buildPie(inUse, idle, fault);

            const bool over = ratio > 20.0;
            m_alarmLabel->setText(over
                ? QStringLiteral("故障预警：故障桩占比 %1%（超过 20% 阈值），请及时排查！")
                      .arg(ratio, 0, 'f', 1)
                : QStringLiteral("运行正常：故障桩占比 %1%（阈值 20%）").arg(ratio, 0, 'f', 1));
            m_alarmLabel->setStyleSheet(Theme::alarmQss(!over));
        });
}
