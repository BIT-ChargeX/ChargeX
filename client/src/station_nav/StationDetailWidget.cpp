#include "StationDetailWidget.h"
#include "common/NetClient.h"
#include "common/ApiDefs.h"

#include <QLabel>
#include <QTableWidget>
#include <QTableWidgetItem>
#include <QPushButton>
#include <QHeaderView>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QJsonArray>
#include <QJsonObject>
#include <QFont>

StationDetailWidget::StationDetailWidget(QWidget* parent) : QDialog(parent) {
    setWindowTitle(QStringLiteral("充电站详情"));
    resize(430, 520);

    auto* layout = new QVBoxLayout(this);
    layout->setSpacing(10);

    m_nameLabel = new QLabel(this);
    QFont f = m_nameLabel->font();
    f.setPointSize(14);
    f.setBold(true);
    m_nameLabel->setFont(f);
    layout->addWidget(m_nameLabel);

    m_addrLabel = new QLabel(this);
    m_addrLabel->setWordWrap(true);
    m_addrLabel->setStyleSheet(QStringLiteral("color: #555;"));
    layout->addWidget(m_addrLabel);

    m_priceLabel = new QLabel(this);
    m_priceLabel->setStyleSheet(QStringLiteral("color: #e65100;"));
    layout->addWidget(m_priceLabel);

    m_pileTable = new QTableWidget(this);
    m_pileTable->setColumnCount(4);
    m_pileTable->setHorizontalHeaderLabels(
        {QStringLiteral("电桩编号"), QStringLiteral("类型"),
         QStringLiteral("状态"), QStringLiteral("功率(kW)")});
    m_pileTable->horizontalHeader()->setSectionResizeMode(QHeaderView::Stretch);
    m_pileTable->setSelectionBehavior(QAbstractItemView::SelectRows);
    m_pileTable->setSelectionMode(QAbstractItemView::SingleSelection);
    m_pileTable->setEditTriggers(QAbstractItemView::NoEditTriggers);
    layout->addWidget(m_pileTable, 1);

    auto* btnRow = new QHBoxLayout;
    m_navBtn = new QPushButton(QStringLiteral("一键导航"), this);
    m_goChargingBtn = new QPushButton(QStringLiteral("去充电"), this);
    auto* closeBtn = new QPushButton(QStringLiteral("关闭"), this);
    btnRow->addWidget(m_navBtn);
    btnRow->addWidget(m_goChargingBtn);
    btnRow->addWidget(closeBtn);
    layout->addLayout(btnRow);

    m_statusLabel = new QLabel(this);
    m_statusLabel->setStyleSheet(QStringLiteral("color: #c62828;"));
    m_statusLabel->setWordWrap(true);
    layout->addWidget(m_statusLabel);

    connect(m_goChargingBtn, &QPushButton::clicked, this, &StationDetailWidget::onGoChargingClicked);
    connect(m_navBtn, &QPushButton::clicked, this, &StationDetailWidget::onNavClicked);
    connect(closeBtn, &QPushButton::clicked, this, &StationDetailWidget::close);

    connect(m_pileTable, &QTableWidget::itemDoubleClicked, this,
            [this](QTableWidgetItem*) { onGoChargingClicked(); });
}

void StationDetailWidget::showStation(const QJsonObject& station) {
    m_station = station;
    m_full = station;
    m_piles.clear();

    m_nameLabel->setText(station.value("name").toString());
    m_addrLabel->setText(station.value("address").toString());
    m_priceLabel->setText(QStringLiteral("充电价格：%1 元/度")
                              .arg(station.value("price").toDouble(), 0, 'f', 2));
    m_statusLabel->clear();
    m_pileTable->setRowCount(0);
    m_goChargingBtn->setEnabled(false);
    m_navBtn->setEnabled(false);

    fillStationHeader(station);
    loadDetail();
    loadPiles();

    show();
}

void StationDetailWidget::fillStationHeader(const QJsonObject& s) {
    m_nameLabel->setText(s.value("name").toString());
    if (s.contains("address")) m_addrLabel->setText(s.value("address").toString());
    if (s.contains("price"))
        m_priceLabel->setText(QStringLiteral("充电价格：%1 元/度")
                                  .arg(s.value("price").toDouble(), 0, 'f', 2));
    if (s.contains("lat") && s.contains("lng")) {
        m_navBtn->setEnabled(true);
    }
}

void StationDetailWidget::loadDetail() {
    QJsonObject data;
    data["station_id"] = m_station.value("station_id").toInt();

    NetClient::instance().sendRequest(Api::CmdStationDetail, data,
        [this](const QJsonObject& resp, int code, const QString& /*msg*/) {
            if (code != 0) return;
            m_full = resp;
            fillStationHeader(resp);
        });
}

void StationDetailWidget::loadPiles() {
    QJsonObject data;
    data["station_id"] = m_station.value("station_id").toInt();
    m_statusLabel->setText(QStringLiteral("正在加载电桩列表…"));
    m_statusLabel->setStyleSheet(QStringLiteral("color: #666;"));

    NetClient::instance().sendRequest(Api::CmdPileDetailList, data,
        [this](const QJsonObject& resp, int code, const QString& msg) {
            if (code != 0) {
                m_statusLabel->setText(QStringLiteral("加载电桩失败：%1").arg(msg));
                return;
            }
            m_piles.clear();
            QJsonArray piles = resp.value("piles").toArray();
            for (const auto& v : piles) m_piles.append(v.toObject());

            m_pileTable->setRowCount(static_cast<int>(m_piles.size()));
            for (int r = 0; r < m_piles.size(); ++r) {
                const QJsonObject& p = m_piles[r];
                QStringList cols = {
                    QString::number(p.value("pile_id").toInt()),
                    p.value("type").toString(),
                    p.value("status").toString(),
                    QString::number(p.value("power").toDouble())
                };
                for (int c = 0; c < cols.size(); ++c) {
                    auto* it = new QTableWidgetItem(cols[c]);
                    it->setTextAlignment(Qt::AlignCenter);
                    m_pileTable->setItem(r, c, it);
                }
            }
            m_statusLabel->clear();
            if (!m_piles.isEmpty()) m_goChargingBtn->setEnabled(true);
        });
}

void StationDetailWidget::onGoChargingClicked() {
    int row = m_pileTable->currentRow();
    if (row < 0 || row >= m_piles.size()) {
        m_statusLabel->setText(QStringLiteral("请先在表格中选择一个电桩"));
        return;
    }
    const QString status = m_piles[row].value("status").toString();
    if (status.contains(QStringLiteral("故障")) || status.contains(QStringLiteral("在用"))) {
        m_statusLabel->setText(QStringLiteral("该电桩当前不可用，请选择状态为【空闲】的电桩"));
        return;
    }
    QJsonObject pile = m_piles[row];
    pile.insert("station_id", m_station.value("station_id").toInt());
    pile.insert("station_name", m_station.value("name").toString());
    emit pilePicked(pile);
    accept();
}

void StationDetailWidget::onNavClicked() {
    if (!m_full.contains("lat") || !m_full.contains("lng")) {
        m_statusLabel->setText(QStringLiteral("站点坐标尚未加载完成，请稍后再试"));
        return;
    }
    emit navRequested(m_full);
}
