#include "StationListWidget.h"
#include "common/NetClient.h"
#include "common/AppSession.h"
#include "common/MapApi.h"
#include "common/ApiDefs.h"

#include <QLineEdit>
#include <QDoubleSpinBox>
#include <QPushButton>
#include <QListWidget>
#include <QListWidgetItem>
#include <QLabel>
#include <QFrame>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QJsonArray>
#include <QJsonObject>
#include <QJsonValue>

StationListWidget::StationListWidget(QWidget* parent) : QWidget(parent) {
    auto* layout = new QVBoxLayout(this);
    layout->setSpacing(8);

    auto* addrRow = new QHBoxLayout;
    m_addressEdit = new QLineEdit(this);
    m_addressEdit->setPlaceholderText(QStringLiteral("输入地址定位，如：北京市海淀区中关村"));
    m_locateAddrBtn = new QPushButton(QStringLiteral("地址定位"), this);
    addrRow->addWidget(m_addressEdit, 1);
    addrRow->addWidget(m_locateAddrBtn);
    layout->addLayout(addrRow);

    auto* coordRow = new QHBoxLayout;
    m_latSpin = new QDoubleSpinBox(this);
    m_latSpin->setRange(-90.0, 90.0);
    m_latSpin->setDecimals(6);
    m_latSpin->setPrefix(QStringLiteral("纬度 "));
    m_lngSpin = new QDoubleSpinBox(this);
    m_lngSpin->setRange(-180.0, 180.0);
    m_lngSpin->setDecimals(6);
    m_lngSpin->setPrefix(QStringLiteral("经度 "));
    m_locateCoordBtn = new QPushButton(QStringLiteral("按坐标查询"), this);
    coordRow->addWidget(m_latSpin);
    coordRow->addWidget(m_lngSpin);
    coordRow->addWidget(m_locateCoordBtn);
    layout->addLayout(coordRow);

    m_statusLabel = new QLabel(this);
    m_statusLabel->setWordWrap(true);
    layout->addWidget(m_statusLabel);

    m_listWidget = new QListWidget(this);
    m_listWidget->setSpacing(4);
    layout->addWidget(m_listWidget, 1);

    m_latSpin->setValue(AppSession::instance().latitude());
    m_lngSpin->setValue(AppSession::instance().longitude());

    connect(m_locateAddrBtn, &QPushButton::clicked, this, &StationListWidget::onLocateByAddress);
    connect(m_locateCoordBtn, &QPushButton::clicked, this, &StationListWidget::onLocateByCoords);
}

void StationListWidget::setStatus(const QString& text, bool ok) {
    m_statusLabel->setText(text);
    m_statusLabel->setStyleSheet(ok
        ? QStringLiteral("color: #666;")
        : QStringLiteral("color: #c62828;"));
}

void StationListWidget::refreshNearby() {
    if (!AppSession::instance().isLoggedIn()) return;
    queryNearby(AppSession::instance().latitude(), AppSession::instance().longitude());
}

void StationListWidget::onLocateByAddress() {
    const QString address = m_addressEdit->text().trimmed();
    if (address.isEmpty()) {
        setStatus(QStringLiteral("请输入地址后再定位"), false);
        return;
    }

    m_locateAddrBtn->setEnabled(false);
    setStatus(QStringLiteral("正在调用腾讯地图定位…"), true);

    MapApi::instance().geocode(address, [this, address](bool ok, double lat, double lng,
                                                        const QString& msg) {
        m_locateAddrBtn->setEnabled(true);
        if (!ok) {
            setStatus(QStringLiteral("定位失败：%1（可改用下方'按坐标查询'手动输入）").arg(msg), false);
            return;
        }
        AppSession::instance().setPosition(lat, lng, address);
        m_latSpin->setValue(lat);
        m_lngSpin->setValue(lng);
        queryNearby(lat, lng);
    });
}

void StationListWidget::onLocateByCoords() {
    double lat = m_latSpin->value();
    double lng = m_lngSpin->value();
    AppSession::instance().setPosition(lat, lng, QStringLiteral("手动坐标"));
    queryNearby(lat, lng);
}

void StationListWidget::queryNearby(double lat, double lng) {
    setStatus(QStringLiteral("正在查询附近充电站…"), true);

    QJsonObject data;
    data["lat"] = lat;
    data["lng"] = lng;

    NetClient::instance().sendRequest(Api::CmdStationNearby, data,
        [this](const QJsonObject& resp, int code, const QString& msg) {
            if (code != 0) {
                setStatus(QStringLiteral("查询失败：%1").arg(msg), false);
                return;
            }

            QJsonArray stations = resp.value("stations").toArray();
            m_listWidget->clear();

            // 距离排序由服务端 STATION_NEARBY 完成，客户端直接按序展示
            for (const auto& v : stations) addStationCard(v.toObject());

            if (stations.isEmpty()) {
                setStatus(QStringLiteral("附近暂无充电站"), false);
            } else {
                setStatus(QStringLiteral("共找到 %1 座充电站（按距离由近及远）").arg(stations.size()), true);
            }
        });
}

void StationListWidget::addStationCard(const QJsonObject& station) {
    auto* item = new QListWidgetItem(m_listWidget);
    item->setSizeHint(QSize(0, 100));

    auto* frame = new QFrame(m_listWidget);
    frame->setFrameShape(QFrame::StyledPanel);
    frame->setStyleSheet(
        QStringLiteral("QFrame { background: #ffffff; border: 1px solid #e0e0e0;"
                       " border-radius: 8px; }"));

    auto* v = new QVBoxLayout(frame);
    v->setContentsMargins(10, 8, 10, 8);
    v->setSpacing(4);

    auto* topRow = new QHBoxLayout;
    auto* name = new QLabel(station.value("name").toString(), frame);
    QFont f = name->font();
    f.setBold(true);
    f.setPointSize(11);
    name->setFont(f);
    topRow->addWidget(name);
    topRow->addStretch(1);
    auto* dist = new QLabel(QStringLiteral("%1 km")
                                .arg(station.value("distance").toDouble(), 0, 'f', 1), frame);
    dist->setStyleSheet(QStringLiteral("color: #1976d2; font-weight: bold;"));
    topRow->addWidget(dist);
    v->addLayout(topRow);

    auto* midRow = new QHBoxLayout;
    midRow->addWidget(new QLabel(
        QStringLiteral("电价 %1 元/度 · 空闲 %2/%3")
            .arg(station.value("price").toDouble(), 0, 'f', 2)
            .arg(station.value("pile_free").toInt())
            .arg(station.value("pile_total").toInt()), frame));
    midRow->addStretch(1);
    v->addLayout(midRow);

    auto* btnRow = new QHBoxLayout;
    btnRow->addStretch(1);
    auto* detailBtn = new QPushButton(QStringLiteral("查看电桩"), frame);
    auto* navBtn = new QPushButton(QStringLiteral("导航"), frame);
    btnRow->addWidget(detailBtn);
    btnRow->addWidget(navBtn);
    v->addLayout(btnRow);

    connect(detailBtn, &QPushButton::clicked, this,
            [this, station]() { emit stationDetailRequested(station); });
    connect(navBtn, &QPushButton::clicked, this,
            [this, station]() { requestDetailForNav(station); });

    m_listWidget->setItemWidget(item, frame);
}

// 列表导航按钮：先取站点经纬度/地址，再交给导航页
void StationListWidget::requestDetailForNav(const QJsonObject& station) {
    setStatus(QStringLiteral("正在获取站点坐标…"), true);
    QJsonObject data;
    data["station_id"] = station.value("station_id").toInt();

    NetClient::instance().sendRequest(Api::CmdStationDetail, data,
        [this, station](const QJsonObject& resp, int code, const QString& msg) {
            if (code != 0) {
                setStatus(QStringLiteral("获取站点坐标失败：%1").arg(msg), false);
                return;
            }
            QJsonObject full = station;
            full.insert("name", resp.value("name").toString());
            full.insert("address", resp.value("address").toString());
            full.insert("lat", resp.value("lat").toDouble());
            full.insert("lng", resp.value("lng").toDouble());
            emit navRequested(full);
        });
}
