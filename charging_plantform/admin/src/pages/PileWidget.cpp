#include "PileWidget.h"
#include "common/NetClient.h"
#include "common/AdminSession.h"
#include "common/ApiDefs.h"
#include "common/Theme.h"

#include <QTableWidget>
#include <QTableWidgetItem>
#include <QHeaderView>
#include <QPushButton>
#include <QLabel>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QSplitter>
#include <QJsonObject>
#include <QJsonArray>
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
    lv->addWidget(m_logTable, 1);

    splitter->addWidget(pilePanel);
    splitter->addWidget(logPanel);
    splitter->setStretchFactor(0, 3);
    splitter->setStretchFactor(1, 2);
    layout->addWidget(splitter, 1);

    connect(m_refreshBtn, &QPushButton::clicked, this, &PileWidget::refresh);
    connect(m_rebootBtn, &QPushButton::clicked, this, &PileWidget::onReboot);
    connect(m_pileTable, &QTableWidget::currentCellChanged,
            this, [this](int, int, int, int) { updateRebootButton(); });
    connect(m_pileTable, &QTableWidget::cellDoubleClicked,
            this, &PileWidget::onRowDoubleClicked);

    // 初始禁用，随选中行状态机开启（闲置/在用才可重启）
    m_rebootBtn->setEnabled(false);

    refresh();
}

void PileWidget::refresh() {
    loadPiles();
    loadOpsLog();
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
