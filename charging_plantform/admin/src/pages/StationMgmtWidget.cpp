#include "StationMgmtWidget.h"
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
#include <QDialog>
#include <QFormLayout>
#include <QLineEdit>
#include <QDoubleSpinBox>
#include <QSpinBox>
#include <QDialogButtonBox>
#include <QJsonObject>
#include <QJsonArray>
#include <QBrush>
#include <QMessageBox>

StationMgmtWidget::StationMgmtWidget(QWidget* parent) : QWidget(parent) {
    auto* layout = new QVBoxLayout(this);

    auto* bar = new QHBoxLayout;
    m_addBtn = new QPushButton(QStringLiteral("新增充电站"), this);
    m_addBtn->setObjectName(QStringLiteral("btnPrimary"));
    m_faultBtn = new QPushButton(QStringLiteral("选中电桩设为故障"), this);
    m_faultBtn->setObjectName(QStringLiteral("btnDanger"));
    m_idleBtn = new QPushButton(QStringLiteral("选中电桩恢复空闲"), this);
    m_idleBtn->setObjectName(QStringLiteral("btnSuccess"));
    m_refreshBtn = new QPushButton(QStringLiteral("刷新"), this);
    m_refreshBtn->setObjectName(QStringLiteral("btnGhost"));
    m_statusLabel = new QLabel(this);
    m_statusLabel->setObjectName(QStringLiteral("cardCaption"));
    bar->addWidget(m_addBtn);
    bar->addWidget(m_faultBtn);
    bar->addWidget(m_idleBtn);
    bar->addWidget(m_refreshBtn);
    bar->addStretch(1);
    bar->addWidget(m_statusLabel);
    layout->addLayout(bar);

    // 初始禁用，随选中行的状态机开启
    m_faultBtn->setEnabled(false);
    m_idleBtn->setEnabled(false);

    auto* splitter = new QSplitter(Qt::Horizontal, this);
    splitter->setChildrenCollapsible(false);

    auto* stPanel = new QWidget(splitter);
    stPanel->setMinimumWidth(520);
    auto* sv = new QVBoxLayout(stPanel);
    auto* stationTitle = new QLabel(QStringLiteral("充电站列表（点击行查看站内电桩）"), stPanel);
    stationTitle->setObjectName(QStringLiteral("sectionTitle"));
    sv->addWidget(stationTitle);
    m_stationTable = new QTableWidget(stPanel);
    m_stationTable->setColumnCount(8);
    m_stationTable->setHorizontalHeaderLabels(
        {QStringLiteral("ID"), QStringLiteral("站名"), QStringLiteral("地址"),
         QStringLiteral("纬度"), QStringLiteral("经度"), QStringLiteral("电价(元/度)"),
         QStringLiteral("桩总数"), QStringLiteral("空闲")});
    m_stationTable->horizontalHeader()->setSectionResizeMode(QHeaderView::Stretch);
    m_stationTable->setSelectionBehavior(QAbstractItemView::SelectRows);
    m_stationTable->setSelectionMode(QAbstractItemView::SingleSelection);
    m_stationTable->setEditTriggers(QAbstractItemView::NoEditTriggers);
    m_stationTable->setAlternatingRowColors(true);
    m_stationTable->verticalHeader()->setVisible(false);
    sv->addWidget(m_stationTable, 1);

    auto* plPanel = new QWidget(splitter);
    plPanel->setMinimumWidth(360);
    auto* pv = new QVBoxLayout(plPanel);
    auto* pileTitle = new QLabel(QStringLiteral("站内电桩（编号 / 类型 / 状态）"), plPanel);
    pileTitle->setObjectName(QStringLiteral("sectionTitle"));
    pv->addWidget(pileTitle);
    m_pileTable = new QTableWidget(plPanel);
    m_pileTable->setColumnCount(4);
    m_pileTable->setHorizontalHeaderLabels(
        {QStringLiteral("电桩ID"), QStringLiteral("编号"), QStringLiteral("类型"), QStringLiteral("状态")});
    m_pileTable->horizontalHeader()->setSectionResizeMode(QHeaderView::Stretch);
    m_pileTable->setSelectionBehavior(QAbstractItemView::SelectRows);
    m_pileTable->setSelectionMode(QAbstractItemView::SingleSelection);
    m_pileTable->setEditTriggers(QAbstractItemView::NoEditTriggers);
    m_pileTable->setAlternatingRowColors(true);
    m_pileTable->verticalHeader()->setVisible(false);
    m_pileTable->setToolTip(QStringLiteral("双击 闲置/在用 电桩可设为故障；故障桩等待报修"));
    pv->addWidget(m_pileTable, 1);

    splitter->addWidget(stPanel);
    splitter->addWidget(plPanel);
    splitter->setStretchFactor(0, 5);
    splitter->setStretchFactor(1, 4);
    layout->addWidget(splitter, 1);

    connect(m_refreshBtn, &QPushButton::clicked, this, &StationMgmtWidget::refresh);
    connect(m_addBtn, &QPushButton::clicked, this, &StationMgmtWidget::onAddStation);
    connect(m_faultBtn, &QPushButton::clicked, this, &StationMgmtWidget::onSetFault);
    connect(m_idleBtn, &QPushButton::clicked, this, &StationMgmtWidget::onSetIdle);
    connect(m_stationTable, &QTableWidget::currentCellChanged,
            this, [this](int row, int, int, int) { onStationRowChanged(row); });
    connect(m_pileTable, &QTableWidget::currentCellChanged,
            this, [this](int, int, int, int) { updatePileButtons(); });
    connect(m_pileTable, &QTableWidget::cellDoubleClicked,
            this, &StationMgmtWidget::onPileTableDoubleClicked);

    refresh();
}

void StationMgmtWidget::refresh() {
    loadStations();
}

void StationMgmtWidget::loadStations() {
    QJsonObject data;
    AdminSession::instance().attach(data);

    NetClient::instance().sendRequest(Api::CmdStationMgmtList, data,
        [this](const QJsonObject& resp, int code, const QString& msg) {
            if (code != 0) {
                m_statusLabel->setText(QStringLiteral("加载失败：%1").arg(msg));
                return;
            }
            const QJsonArray stations = resp.value("stations").toArray();
            m_stationTable->setRowCount(stations.size());
            for (int r = 0; r < stations.size(); ++r) {
                const QJsonObject s = stations.at(r).toObject();
                const QStringList cols = {
                    QString::number(s.value("station_id").toInt()),
                    s.value("name").toString(),
                    s.value("address").toString(),
                    QString::number(s.value("lat").toDouble()),
                    QString::number(s.value("lng").toDouble()),
                    QString::number(s.value("price").toDouble(), 'f', 2),
                    QString::number(s.value("pile_total").toInt()),
                    QString::number(s.value("pile_free").toInt()),
                };
                for (int c = 0; c < cols.size(); ++c) {
                    auto* it = new QTableWidgetItem(cols.at(c));
                    it->setTextAlignment(Qt::AlignCenter);
                    m_stationTable->setItem(r, c, it);
                }
                m_stationTable->item(r, 0)->setData(Qt::UserRole, s.value("station_id").toInt());
            }

            int selectRow = -1;
            for (int r = 0; r < m_stationTable->rowCount(); ++r) {
                if (m_stationTable->item(r, 0)->data(Qt::UserRole).toInt() == m_currentStationId) {
                    selectRow = r;
                    break;
                }
            }
            if (selectRow < 0 && m_stationTable->rowCount() > 0) selectRow = 0;
            if (selectRow >= 0) {
                m_stationTable->selectRow(selectRow);
                onStationRowChanged(selectRow);
            } else {
                m_pileTable->setRowCount(0);
            }
            m_statusLabel->setText(QStringLiteral("共 %1 座充电站").arg(stations.size()));
        });
}

void StationMgmtWidget::onStationRowChanged(int row) {
    if (row < 0 || !m_stationTable->item(row, 0)) return;
    const int stationId = m_stationTable->item(row, 0)->data(Qt::UserRole).toInt();
    if (stationId != m_currentStationId) {
        m_currentStationId = stationId;
        loadPilesOfStation(stationId);
    }
}

void StationMgmtWidget::loadPilesOfStation(int stationId) {
    QJsonObject data;
    AdminSession::instance().attach(data);
    data["station_id"] = stationId;

    NetClient::instance().sendRequest(Api::CmdPileMgmtList, data,
        [this](const QJsonObject& resp, int code, const QString& msg) {
            if (code != 0) {
                m_pileTable->setRowCount(0);
                return;
            }
            const QJsonArray piles = resp.value("piles").toArray();
            m_pileTable->setRowCount(piles.size());
            for (int r = 0; r < piles.size(); ++r) {
                const QJsonObject p = piles.at(r).toObject();
                const QString status = p.value("status").toString();
                const QStringList cols = {
                    QString::number(p.value("pile_id").toInt()),
                    p.value("code").toString(),
                    p.value("type").toString(),
                    status,
                };
                for (int c = 0; c < cols.size(); ++c) {
                    auto* it = new QTableWidgetItem(cols.at(c));
                    it->setTextAlignment(Qt::AlignCenter);
                    m_pileTable->setItem(r, c, it);
                }
                m_pileTable->item(r, 0)->setData(Qt::UserRole, p.value("pile_id").toInt());

                QTableWidgetItem* stItem = m_pileTable->item(r, 3);
                stItem->setForeground(QBrush(Theme::statusText(status)));
                stItem->setBackground(QBrush(Theme::statusBackground(status)));
            }
        });
}

void StationMgmtWidget::setPileStatus(int pileId, const QString& status) {
    QJsonObject data;
    AdminSession::instance().attach(data);
    data["pile_id"] = pileId;
    data["status"] = status;

    NetClient::instance().sendRequest(Api::CmdPileMgmtSetStatus, data,
        [this](const QJsonObject&, int code, const QString& msg) {
            if (code != 0) {
                QMessageBox::warning(this, QStringLiteral("操作失败"), msg);
                return;
            }
            const int stationId = m_currentStationId;
            loadStations();
            if (stationId > 0) loadPilesOfStation(stationId);
        });
}

void StationMgmtWidget::onSetFault() {
    const int row = m_pileTable->currentRow();
    if (row < 0 || !m_pileTable->item(row, 0)) return;
    setPileStatus(m_pileTable->item(row, 0)->data(Qt::UserRole).toInt(),
                  QStringLiteral("故障"));
}

void StationMgmtWidget::onSetIdle() {
    const int row = m_pileTable->currentRow();
    if (row < 0 || !m_pileTable->item(row, 0)) return;
    setPileStatus(m_pileTable->item(row, 0)->data(Qt::UserRole).toInt(),
                  QStringLiteral("闲置"));
}

void StationMgmtWidget::onPileTableDoubleClicked(int row, int column) {
    Q_UNUSED(column)
    if (row < 0 || !m_pileTable->item(row, 0) || !m_pileTable->item(row, 3)) return;

    const int pileId = m_pileTable->item(row, 0)->data(Qt::UserRole).toInt();
    const QString status = m_pileTable->item(row, 3)->text();

    // 双击仅用于“闲置/在用 → 故障”；故障等待报修、预约占用禁止一切变更
    if (status != QStringLiteral("闲置") && status != QStringLiteral("在用")) {
        const QString hint = status == QStringLiteral("故障")
            ? QStringLiteral("电桩故障等待报修，禁止设故障/恢复空闲，请先线下检修")
            : QStringLiteral("电桩被预约占用，禁止变更状态");
        QMessageBox::information(this, QStringLiteral("设为故障"), hint);
        return;
    }

    const QString code = m_pileTable->item(row, 1)->text();
    if (QMessageBox::question(this, QStringLiteral("设为故障"),
                              QStringLiteral("确定将电桩 %1（%2）设为故障吗？")
                                  .arg(pileId).arg(code),
                              QMessageBox::Yes | QMessageBox::No,
                              QMessageBox::No) != QMessageBox::Yes) {
        return;
    }
    setPileStatus(pileId, QStringLiteral("故障"));
}

void StationMgmtWidget::updatePileButtons() {
    const int row = m_pileTable->currentRow();
    const bool has = row >= 0 && m_pileTable->item(row, 3);
    const QString status = has ? m_pileTable->item(row, 3)->text() : QString();
    // 设故障：闲置/在用；恢复空闲：仅在用；故障=等待报修（不提供操作）
    m_faultBtn->setEnabled(status == QStringLiteral("闲置")
                           || status == QStringLiteral("在用"));
    m_idleBtn->setEnabled(status == QStringLiteral("在用"));
}

void StationMgmtWidget::onAddStation() {
    QDialog dlg(this);
    dlg.setWindowTitle(QStringLiteral("新增充电站（提交服务器创建）"));
    auto* form = new QFormLayout;
    auto* name = new QLineEdit(&dlg);
    auto* addr = new QLineEdit(&dlg);
    auto* lat = new QDoubleSpinBox(&dlg);   lat->setRange(-90, 90); lat->setDecimals(6);
    auto* lng = new QDoubleSpinBox(&dlg);   lng->setRange(-180, 180); lng->setDecimals(6);
    auto* price = new QDoubleSpinBox(&dlg); price->setRange(0.1, 10.0); price->setDecimals(2); price->setValue(1.5);
    auto* count = new QSpinBox(&dlg);       count->setRange(1, 50); count->setValue(4);
    form->addRow(QStringLiteral("站名"), name);
    form->addRow(QStringLiteral("地址"), addr);
    form->addRow(QStringLiteral("纬度"), lat);
    form->addRow(QStringLiteral("经度"), lng);
    form->addRow(QStringLiteral("电价(元/度)"), price);
    form->addRow(QStringLiteral("电桩数量"), count);
    auto* buttons = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel, &dlg);
    connect(buttons, &QDialogButtonBox::accepted, &dlg, &QDialog::accept);
    connect(buttons, &QDialogButtonBox::rejected, &dlg, &QDialog::reject);
    auto* root = new QVBoxLayout(&dlg);
    root->addLayout(form);
    root->addWidget(buttons);

    if (dlg.exec() != QDialog::Accepted) return;
    if (name->text().trimmed().isEmpty() || addr->text().trimmed().isEmpty()) {
        QMessageBox::warning(this, QStringLiteral("提示"), QStringLiteral("站名与地址不能为空"));
        return;
    }

    QJsonObject data;
    AdminSession::instance().attach(data);
    data["name"] = name->text().trimmed();
    data["address"] = addr->text().trimmed();
    data["lat"] = lat->value();
    data["lng"] = lng->value();
    data["price"] = price->value();
    data["pile_count"] = count->value();

    NetClient::instance().sendRequest(Api::CmdStationMgmtAdd, data,
        [this](const QJsonObject&, int code, const QString& msg) {
            if (code != 0) {
                QMessageBox::warning(this, QStringLiteral("新增失败"), msg);
                return;
            }
            m_currentStationId = -1;
            loadStations();
        });
}
