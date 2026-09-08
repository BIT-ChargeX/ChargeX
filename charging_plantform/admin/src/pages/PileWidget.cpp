#include "PileWidget.h"
#include "common/NetClient.h"
#include "common/AdminSession.h"
#include "common/ApiDefs.h"
#include "common/Theme.h"
#include "common/SimpleCharts.h"

#include <QTableWidget>
#include <QTableWidgetItem>
#include <QHeaderView>
#include <QPushButton>
#include <QLabel>
#include <QComboBox>
#include <QTimer>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QSplitter>
#include <QJsonObject>
#include <QJsonArray>
#include <QVector>
#include <QBrush>
#include <QMessageBox>

PileWidget::PileWidget(QWidget* parent) : QWidget(parent) {
    auto* layout = new QVBoxLayout(this);

    auto* bar = new QHBoxLayout;
    m_rebootBtn = new QPushButton(QStringLiteral("远程重启选中电桩"), this);
    m_rebootBtn->setObjectName(QStringLiteral("btnPrimary"));
    m_refreshBtn = new QPushButton(QStringLiteral("刷新"), this);
    m_refreshBtn->setObjectName(QStringLiteral("btnGhost"));
    m_countLabel = new QLabel(this);
    m_countLabel->setObjectName(QStringLiteral("cardCaption"));
    bar->addWidget(m_rebootBtn);
    bar->addWidget(m_refreshBtn);
    bar->addStretch(1);
    bar->addWidget(m_countLabel);
    layout->addLayout(bar);

    auto* splitter = new QSplitter(Qt::Vertical, this);

    auto* pilePanel = new QWidget(splitter);
    auto* pv = new QVBoxLayout(pilePanel);
    auto* pileTitle = new QLabel(QStringLiteral("电桩列表（所属站 / 编号 / 类型 / 状态 / 累计）"), pilePanel);
    pileTitle->setObjectName(QStringLiteral("sectionTitle"));
    pv->addWidget(pileTitle);
    m_pileTable = new QTableWidget(pilePanel);
    m_pileTable->setColumnCount(8);
    m_pileTable->setHorizontalHeaderLabels(
        {QStringLiteral("电桩ID"), QStringLiteral("所属充电站"), QStringLiteral("编号"),
         QStringLiteral("类型"), QStringLiteral("功率(kW)"), QStringLiteral("状态"),
         QStringLiteral("累计次数"), QStringLiteral("累计时长(h)")});
    m_pileTable->horizontalHeader()->setSectionResizeMode(QHeaderView::Stretch);
    m_pileTable->setSelectionBehavior(QAbstractItemView::SelectRows);
    m_pileTable->setSelectionMode(QAbstractItemView::SingleSelection);
    m_pileTable->setEditTriggers(QAbstractItemView::NoEditTriggers);
    m_pileTable->setAlternatingRowColors(true);
    m_pileTable->verticalHeader()->setVisible(false);
    m_pileTable->setToolTip(QStringLiteral("双击 闲置/在用 电桩可远程重启；故障桩等待报修"));
    pv->addWidget(m_pileTable, 1);

    auto* logPanel = new QWidget(splitter);
    auto* lv = new QVBoxLayout(logPanel);
    auto* logTitle = new QLabel(QStringLiteral("操作日志"), logPanel);
    logTitle->setObjectName(QStringLiteral("sectionTitle"));
    lv->addWidget(logTitle);
    m_logTable = new QTableWidget(logPanel);
    m_logTable->setColumnCount(4);
    m_logTable->setHorizontalHeaderLabels(
        {QStringLiteral("时间"), QStringLiteral("电桩ID"), QStringLiteral("操作人"), QStringLiteral("动作")});
    m_logTable->horizontalHeader()->setSectionResizeMode(QHeaderView::Stretch);
    m_logTable->setEditTriggers(QAbstractItemView::NoEditTriggers);
    m_logTable->setAlternatingRowColors(true);
    m_logTable->verticalHeader()->setVisible(false);
    m_logTable->setFixedHeight(120);
    lv->addWidget(m_logTable);
    lv->addSpacing(10);

    // 功率-时间曲线卡（选中电桩后显示，5s 自动刷新）
    auto* trendHead = new QHBoxLayout;
    m_trendTitle = new QLabel(QStringLiteral("功率曲线（未选择电桩）"), logPanel);
    m_trendTitle->setObjectName(QStringLiteral("sectionTitle"));
    trendHead->addWidget(m_trendTitle);
    trendHead->addStretch(1);
    m_trendRange = new QComboBox(logPanel);
    m_trendRange->addItem(QStringLiteral("近 5 分钟"), 5);
    m_trendRange->addItem(QStringLiteral("近 30 分钟"), 30);
    m_trendRange->addItem(QStringLiteral("近 1 小时"), 60);
    m_trendRange->addItem(QStringLiteral("近 24 小时"), 1440);
    m_trendRange->setCurrentIndex(2);   // 默认近 1 小时
    trendHead->addWidget(m_trendRange);
    lv->addLayout(trendHead);
    m_line = new LineChartWidget(logPanel);
    lv->addWidget(m_line, 1);

    splitter->addWidget(pilePanel);
    splitter->addWidget(logPanel);
    splitter->setStretchFactor(0, 4);
    splitter->setStretchFactor(1, 5);
    layout->addWidget(splitter, 1);

    connect(m_refreshBtn, &QPushButton::clicked, this, &PileWidget::refresh);
    connect(m_rebootBtn, &QPushButton::clicked, this, &PileWidget::onReboot);
    connect(m_pileTable, &QTableWidget::currentCellChanged,
            this, [this](int, int, int, int) {
                updateRebootButton();
                loadTrend();
            });
    connect(m_pileTable, &QTableWidget::cellDoubleClicked,
            this, &PileWidget::onRowDoubleClicked);
    connect(m_trendRange, qOverload<int>(&QComboBox::currentIndexChanged),
            this, [this](int) { loadTrend(); });

    // 初始禁用，随选中行状态机开启（闲置/在用才可重启）
    m_rebootBtn->setEnabled(false);

    // 曲线自动刷新：3 秒推进一次时间轴（数据按终端上报节奏更新）
    m_trendTimer = new QTimer(this);
    m_trendTimer->setInterval(3000);
    connect(m_trendTimer, &QTimer::timeout, this, &PileWidget::loadTrend);
    m_trendTimer->start();

    refresh();
}

void PileWidget::refresh() {
    loadPiles();
    loadOpsLog();
    loadTrend();
}

void PileWidget::loadPiles() {
    QJsonObject data;
    AdminSession::instance().attach(data);
    data["page"] = 1;

    NetClient::instance().sendRequest(Api::CmdPileMgmtList, data,
        [this](const QJsonObject& resp, int code, const QString& msg) {
            if (code != 0) {
                m_countLabel->setText(QStringLiteral("加载失败：%1").arg(msg));
                return;
            }
            const QJsonArray piles = resp.value("piles").toArray();
            m_pileTable->setRowCount(piles.size());
            for (int i = 0; i < piles.size(); ++i) {
                const QJsonObject p = piles.at(i).toObject();
                const int pileId = p.value("pile_id").toInt();
                const QString status = p.value("status").toString();
                const QStringList cols = {
                    QString::number(pileId),
                    p.value("station").toString(),
                    p.value("code").toString(),
                    p.value("type").toString(),
                    QString::number(p.value("power").toDouble()),
                    status,
                    QString::number(p.value("total_times").toInt()),
                    QString::number(p.value("total_hours").toDouble()),
                };
                for (int c = 0; c < cols.size(); ++c) {
                    auto* it = new QTableWidgetItem(cols.at(c));
                    it->setTextAlignment(Qt::AlignCenter);
                    m_pileTable->setItem(i, c, it);
                }
                m_pileTable->item(i, 0)->setData(Qt::UserRole, pileId);

                QTableWidgetItem* stItem = m_pileTable->item(i, 5);
                stItem->setForeground(QBrush(Theme::statusText(status)));
                stItem->setBackground(QBrush(Theme::statusBackground(status)));
            }
            m_countLabel->setText(QStringLiteral("共 %1 台电桩").arg(piles.size()));
            // 默认选中策略：优先选中第一台“在用”桩（便于直接看实时功率曲线），否则选首行；
            // 用户已手动选中时保持不变
            if (m_pileTable->currentRow() < 0 && m_pileTable->rowCount() > 0) {
                int sel = 0;
                for (int i = 0; i < m_pileTable->rowCount(); ++i) {
                    if (m_pileTable->item(i, 5)
                        && m_pileTable->item(i, 5)->text() == QStringLiteral("在用")) {
                        sel = i;
                        break;
                    }
                }
                m_pileTable->selectRow(sel);
            }
        });
}

void PileWidget::loadOpsLog() {
    QJsonObject data;
    AdminSession::instance().attach(data);

    NetClient::instance().sendRequest(Api::CmdOpsLogList, data,
        [this](const QJsonObject& resp, int code, const QString& /*msg*/) {
            if (code != 0) {
                m_logTable->setRowCount(0);
                return;
            }
            const QJsonArray logs = resp.value("logs").toArray();
            m_logTable->setRowCount(logs.size());
            for (int r = 0; r < logs.size(); ++r) {
                const QJsonObject o = logs.at(r).toObject();
                const QStringList cols = {
                    o.value("time").toString(),
                    QString::number(o.value("pile_id").toInt()),
                    o.value("operator").toString(),
                    o.value("action").toString(),
                };
                for (int c = 0; c < cols.size(); ++c) {
                    auto* it = new QTableWidgetItem(cols.at(c));
                    m_logTable->setItem(r, c, it);
                }
            }
        });
}

void PileWidget::onReboot() {
    const int row = m_pileTable->currentRow();
    if (row < 0) {
        QMessageBox::information(this, QStringLiteral("提示"), QStringLiteral("请先选中一个电桩"));
        return;
    }
    if (!canRebootRow(row)) {
        const QString status = m_pileTable->item(row, 5)->text();
        QMessageBox::information(this, QStringLiteral("远程重启"),
            status == QStringLiteral("故障")
                ? QStringLiteral("电桩故障等待报修，禁止远程重启，请先线下检修")
                : QStringLiteral("电桩被预约占用，禁止远程重启"));
        return;
    }
    const int pileId = m_pileTable->item(row, 0)->data(Qt::UserRole).toInt();
    doReboot(pileId);
}

void PileWidget::onRowDoubleClicked(int row, int column) {
    Q_UNUSED(column)
    if (row < 0 || !m_pileTable->item(row, 0)) return;
    if (!canRebootRow(row)) {
        const QString status = m_pileTable->item(row, 5)->text();
        QMessageBox::information(this, QStringLiteral("远程重启"),
            status == QStringLiteral("故障")
                ? QStringLiteral("电桩故障等待报修，禁止远程重启，请先线下检修")
                : QStringLiteral("电桩被预约占用，禁止远程重启"));
        return;
    }
    const int pileId = m_pileTable->item(row, 0)->data(Qt::UserRole).toInt();
    if (QMessageBox::question(this, QStringLiteral("远程重启"),
                              QStringLiteral("确定远程重启电桩 %1 吗？重启后恢复为【闲置】。")
                                  .arg(pileId),
                              QMessageBox::Yes | QMessageBox::No,
                              QMessageBox::No) != QMessageBox::Yes) {
        return;
    }
    doReboot(pileId);
}

void PileWidget::updateRebootButton() {
    m_rebootBtn->setEnabled(canRebootRow(m_pileTable->currentRow()));
}

bool PileWidget::canRebootRow(int row) {
    if (row < 0 || !m_pileTable->item(row, 5)) return false;
    const QString s = m_pileTable->item(row, 5)->text();
    // 远程重启仅适用于 闲置/在用；故障等待报修、预约占用不可
    return s == QStringLiteral("闲置") || s == QStringLiteral("在用");
}

int PileWidget::currentPileId() {
    const int row = m_pileTable->currentRow();
    if (row < 0 || !m_pileTable->item(row, 0)) return 0;
    return m_pileTable->item(row, 0)->data(Qt::UserRole).toInt();
}

void PileWidget::loadTrend() {
    const int pileId = currentPileId();
    if (pileId <= 0) {
        m_line->setSeries({}, {});
        m_trendTitle->setText(QStringLiteral("功率曲线（未选择电桩）"));
        return;
    }

    QString code;
    const int row = m_pileTable->currentRow();
    if (row >= 0 && m_pileTable->item(row, 2))
        code = m_pileTable->item(row, 2)->text();
    m_trendTitle->setText(QStringLiteral("功率曲线（电桩 %1 · %2）")
                              .arg(pileId).arg(code.isEmpty() ? QStringLiteral("-") : code));

    QJsonObject data;
    AdminSession::instance().attach(data);
    data["pile_id"] = pileId;
    data["minutes"] = m_trendRange->currentData().toInt();

    NetClient::instance().sendRequest(Api::CmdPilePowerTrend, data,
        [this](const QJsonObject& resp, int code, const QString& /*msg*/) {
            if (code != 0) {
                m_line->setSeries({}, {});
                return;
            }
            QVector<qint64> ts;
            QVector<double> power;
            const QJsonArray points = resp.value("points").toArray();
            ts.reserve(points.size());
            power.reserve(points.size());
            for (const auto& v : points) {
                const QJsonObject pt = v.toObject();
                ts.append(pt.value("ts").toVariant().toLongLong());
                power.append(pt.value("power").toDouble());
            }
            m_line->setSeries(ts, power);
        });
}

void PileWidget::doReboot(int pileId) {
    QJsonObject data;
    AdminSession::instance().attach(data);
    data["pile_id"] = pileId;

    NetClient::instance().sendRequest(Api::CmdPileMgmtReboot, data,
        [this, pileId](const QJsonObject&, int code, const QString& msg) {
            if (code != 0) {
                QMessageBox::warning(this, QStringLiteral("远程重启"), msg);
                return;
            }
            QMessageBox::information(this, QStringLiteral("远程重启"),
                                     QStringLiteral("已向电桩 %1 下发重启指令，状态恢复为【闲置】。")
                                         .arg(pileId));
            refresh();
        });
}
