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
#include <QProgressBar>
#include <QDateTime>
#include <QFont>
#include <QBrush>
#include <QAbstractItemView>
#include <QtMath>
#include <QList>
#include <QPair>
#include <QColor>
#include <QJsonObject>
#include <QJsonArray>

namespace {

// 状态卡 + 可选 footer（如故障率进度条）
QLabel* makeStatCard(const QString& caption, const QString& accentName,
                     QWidget* parent, QWidget** boxOut, QWidget* footer = nullptr) {
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
    if (footer) {
        v->addSpacing(2);
        v->addWidget(footer);
    }
    if (boxOut) *boxOut = box;
    return value;
}

// ISO "yyyy-MM-ddTHH:mm:ss" -> "MM-dd HH:mm:ss"
QString compactTs(const QString& iso) {
    QDateTime dt = QDateTime::fromString(iso, Qt::ISODate);
    return dt.isValid() ? dt.toString(QStringLiteral("MM-dd HH:mm:ss")) : iso;
}

} // namespace

MonitorWidget::MonitorWidget(QWidget* parent) : QWidget(parent) {
    auto* layout = new QVBoxLayout(this);
    layout->setSpacing(12);

    // 页头：最后刷新时间（右对齐）
    auto* headRow = new QHBoxLayout;
    headRow->addStretch(1);
    m_refreshLabel = new QLabel(this);
    m_refreshLabel->setObjectName(QStringLiteral("hintMuted"));
    headRow->addWidget(m_refreshLabel);
    layout->addLayout(headRow);

    // 统计卡行（故障卡带故障率进度条）
    m_faultBar = new QProgressBar(this);
    m_faultBar->setRange(0, 100);
    m_faultBar->setValue(0);
    m_faultBar->setTextVisible(false);
    m_faultBar->setFixedHeight(6);
    m_faultBar->setStyleSheet(QStringLiteral(
        "QProgressBar{background:#E4EAF0;border:none;border-radius:3px;}"
        "QProgressBar::chunk{background:%1;border-radius:3px;}")
                                  .arg(Theme::danger().name()));

    QWidget* boxInUse = nullptr;
    QWidget* boxIdle = nullptr;
    QWidget* boxFault = nullptr;
    m_inUseValue = makeStatCard(QStringLiteral("在用（含预约）"), QStringLiteral("Sky"), this, &boxInUse);
    m_idleValue = makeStatCard(QStringLiteral("闲置"), QStringLiteral("Green"), this, &boxIdle);
    m_faultValue = makeStatCard(QStringLiteral("故障"), QStringLiteral("Red"), this, &boxFault, m_faultBar);

    auto* statRow = new QHBoxLayout;
    statRow->addWidget(boxInUse);
    statRow->addWidget(boxIdle);
    statRow->addWidget(boxFault);
    layout->addLayout(statRow);

    // 中部：状态分布明细 + 环形图
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
    auto* chartTitle = new QLabel(QStringLiteral("状态分布（点图例显隐分类）"), m_chartArea);
    chartTitle->setObjectName(QStringLiteral("sectionTitle"));
    cv->addWidget(chartTitle);
    m_pie = new PieChartWidget(m_chartArea);
    cv->addWidget(m_pie, 1);
    splitter->addWidget(m_chartArea);
    splitter->setStretchFactor(0, 1);
    splitter->setStretchFactor(1, 1);
    layout->addWidget(splitter, 1);

    // 阈值预警条
    m_alarmLabel = new QLabel(this);
    m_alarmLabel->setObjectName(QStringLiteral("alarmBox"));
    layout->addWidget(m_alarmLabel);

    // 实时事件流（终端上报 / 指令回执）
    auto* eventCard = new QFrame(this);
    eventCard->setObjectName(QStringLiteral("card"));
    auto* ev = new QVBoxLayout(eventCard);
    ev->setContentsMargins(16, 12, 16, 12);
    ev->setSpacing(8);
    auto* eventTitle = new QLabel(QStringLiteral("最近事件（终端上报 / 指令回执）"), eventCard);
    eventTitle->setObjectName(QStringLiteral("sectionTitle"));
    ev->addWidget(eventTitle);
    m_eventTable = new QTableWidget(eventCard);
    m_eventTable->setColumnCount(6);
    m_eventTable->setHorizontalHeaderLabels(
        {QStringLiteral("时间"), QStringLiteral("设备"), QStringLiteral("电桩"),
         QStringLiteral("事件"), QStringLiteral("状态"), QStringLiteral("说明")});
    m_eventTable->horizontalHeader()->setSectionResizeMode(QHeaderView::Stretch);
    m_eventTable->setSelectionBehavior(QAbstractItemView::SelectRows);
    m_eventTable->setEditTriggers(QAbstractItemView::NoEditTriggers);
    m_eventTable->setAlternatingRowColors(true);
    m_eventTable->verticalHeader()->setVisible(false);
    m_eventTable->setFixedHeight(150);
    ev->addWidget(m_eventTable);
    layout->addWidget(eventCard);

    m_timer = new QTimer(this);
    m_timer->setInterval(5000);
    connect(m_timer, &QTimer::timeout, this, &MonitorWidget::refresh);
    m_timer->start();

    refresh();
}

void MonitorWidget::refresh() {
    loadSummary();
    loadEvents();
}

void MonitorWidget::loadSummary() {
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
            m_faultBar->setValue(qBound(0, static_cast<int>(qRound(ratio)), 100));
            m_refreshLabel->setText(QStringLiteral("最后刷新 %1")
                                        .arg(QDateTime::currentDateTime()
                                                 .toString(QStringLiteral("HH:mm:ss"))));

            struct Row { QString name; int count; };
            QList<Row> rows = {
                {QStringLiteral("在用（含预约）"), inUse},
                {QStringLiteral("闲置"), idle},
                {QStringLiteral("故障"), fault},
            };

            // 明细（含合计行）
            m_table->setRowCount(rows.size() + 1);
            for (int i = 0; i < rows.size(); ++i) {
                double pct = total > 0 ? rows[i].count * 100.0 / total : 0.0;
                QStringList cols = {rows[i].name, QString::number(rows[i].count),
                                    QStringLiteral("%1%").arg(pct, 0, 'f', 1)};
                for (int c = 0; c < cols.size(); ++c) {
                    auto* it = new QTableWidgetItem(cols.at(c));
                    it->setTextAlignment(Qt::AlignCenter);
                    m_table->setItem(i, c, it);
                }
                m_table->item(i, 0)->setForeground(QBrush(Theme::textSecondary()));
            }
            const int last = rows.size();
            const QStringList totalCols = {
                QStringLiteral("合计"), QString::number(total),
                total > 0 ? QStringLiteral("100%") : QStringLiteral("0%"),
            };
            for (int c = 0; c < totalCols.size(); ++c) {
                auto* it = new QTableWidgetItem(totalCols.at(c));
                it->setTextAlignment(Qt::AlignCenter);
                QFont f = it->font();
                f.setBold(true);
                it->setFont(f);
                it->setForeground(QBrush(Theme::textPrimary()));
                m_table->setItem(last, c, it);
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

void MonitorWidget::loadEvents() {
    QJsonObject data;
    AdminSession::instance().attach(data);
    data["limit"] = 10;

    NetClient::instance().sendRequest(Api::CmdPileRuntimeLogList, data,
        [this](const QJsonObject& resp, int code, const QString& /*msg*/) {
            if (code != 0) {
                m_eventTable->setRowCount(0);
                return;
            }
            const QJsonArray logs = resp.value("logs").toArray();
            m_eventTable->setRowCount(logs.size());
            for (int r = 0; r < logs.size(); ++r) {
                const QJsonObject o = logs.at(r).toObject();
                const int pileId = o.value("pile_id").toInt();
                const QString code = o.value("code").toString();
                const QString status = o.value("status").toString();
                QStringList cols = {
                    compactTs(o.value("ts").toString()),
                    o.value("device_id").toString(),
                    pileId > 0 ? QStringLiteral("%1 (%2)").arg(pileId).arg(code)
                               : QStringLiteral("-"),
                    o.value("event").toString(),
                    status.isEmpty() ? QStringLiteral("-") : status,
                    o.value("detail").toString(),
                };
                for (int c = 0; c < cols.size(); ++c) {
                    auto* it = new QTableWidgetItem(cols.at(c));
                    it->setTextAlignment(c == 0 ? Qt::AlignLeft : Qt::AlignCenter);
                    m_eventTable->setItem(r, c, it);
                }
                if (!status.isEmpty()) {
                    QTableWidgetItem* st = m_eventTable->item(r, 4);
                    st->setForeground(QBrush(Theme::statusText(status)));
                    st->setBackground(QBrush(Theme::statusBackground(status)));
                }
            }
        });
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
