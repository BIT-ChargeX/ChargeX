#include "StationListWidget.h"
#include "common/NetClient.h"
#include "common/AppSession.h"
#include "common/MapApi.h"
#include "common/ApiDefs.h"

#include <QLineEdit>
#include <QDoubleSpinBox>
#include <QPushButton>
#include <QComboBox>
#include <QCompleter>
#include <QStringListModel>
#include <QTimer>
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

    // 需求2：下拉选择区域确定当前位置（预置北京各区中心经纬度，无需地图 key 即可用）
    auto* regionRow = new QHBoxLayout;
    regionRow->addWidget(new QLabel(QStringLiteral("区域"), this));
    m_regionCombo = new QComboBox(this);
    const struct { const char* name; double lat; double lng; } kRegions[] = {
        {"北京市（默认）", 39.908823, 116.397470},
        {"海淀区", 39.9593, 116.2981},
        {"朝阳区", 39.9219, 116.4436},
        {"东城区", 39.9284, 116.4169},
        {"西城区", 39.9123, 116.3660},
        {"昌平区", 40.2209, 116.2312},
        {"石景山区", 39.9067, 116.2229},
        {"丰台区", 39.8584, 116.2870},
        {"通州区", 39.9097, 116.6573},
    };
    for (const auto& r : kRegions) {
        m_regionCombo->addItem(QString::fromUtf8(r.name),
                               QStringLiteral("%1,%2").arg(r.lat, 0, 'f', 6).arg(r.lng, 0, 'f', 6));
    }
    regionRow->addWidget(m_regionCombo, 1);
    layout->addLayout(regionRow);

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
    connect(m_regionCombo, QOverload<int>::of(&QComboBox::currentIndexChanged),
            this, &StationListWidget::onRegionSelected);

    // 地址联想下拉：输入关键字 -> 300ms 防抖 -> 腾讯地点联想 -> QCompleter 弹出候选
    m_suggestModel = new QStringListModel(this);
    m_suggestCompleter = new QCompleter(m_suggestModel, this);
    m_suggestCompleter->setCaseSensitivity(Qt::CaseInsensitive);
    m_suggestCompleter->setCompletionMode(QCompleter::PopupCompletion);
    m_suggestCompleter->setMaxVisibleItems(8);
    m_addressEdit->setCompleter(m_suggestCompleter);

    m_suggestTimer = new QTimer(this);
    m_suggestTimer->setSingleShot(true);
    m_suggestTimer->setInterval(300);
    connect(m_suggestTimer, &QTimer::timeout, this, &StationListWidget::doSuggest);
    connect(m_addressEdit, &QLineEdit::textChanged, this, &StationListWidget::onAddressTextChanged);
    connect(m_suggestCompleter, QOverload<const QString&>::of(&QCompleter::activated),
            this, &StationListWidget::onSuggestionPicked);

    // 需求4：点击整张卡片即进入该充电站详情
    connect(m_listWidget, &QListWidget::itemClicked, this, [this](QListWidgetItem* item) {
        if (!item) return;
        emit stationDetailRequested(item->data(Qt::UserRole).toJsonObject());
    });
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

void StationListWidget::onRegionSelected(int index) {
    if (index < 0) return;
    const QString data = m_regionCombo->itemData(index).toString();
    const QStringList parts = data.split(QLatin1Char(','));
    if (parts.size() != 2) return;

    const double lat = parts[0].toDouble();
    const double lng = parts[1].toDouble();
    AppSession::instance().setPosition(lat, lng, m_regionCombo->currentText());
    m_latSpin->setValue(lat);
    m_lngSpin->setValue(lng);
    queryNearby(lat, lng);
}

void StationListWidget::onAddressTextChanged(const QString& text) {
    if (m_suppressSuggest) return;
    if (text.trimmed().size() < 2) {
        m_suggestTimer->stop();
        m_suggestModel->setStringList({});
        return;
    }
    m_suggestTimer->start();   // 防抖：停止输入 300ms 后才发请求
}

void StationListWidget::doSuggest() {
    const QString keyword = m_addressEdit->text().trimmed();
    if (keyword.size() < 2) return;

    // 联想范围固定为北京（与演示数据一致）；region 参数预留给 MapApi 扩展
    MapApi::instance().suggest(keyword, QStringLiteral("北京"),
        [this](bool ok, const QJsonArray& items, const QString& /*msg*/) {
            if (!ok) {
                m_suggestModel->setStringList({});
                return;
            }
            QStringList titles;
            m_suggestItems.clear();
            for (const auto& v : items) {
                const QJsonObject it = v.toObject();
                const QString title = it.value("title").toString();
                const QString addr = it.value("address").toString();
                // 标题可能重复（不同城区同名地点），拼上地址用于区分
                const QString display = addr.isEmpty() ? title
                                                       : QStringLiteral("%1（%2）").arg(title, addr);
                m_suggestItems.insert(display, it);
                titles << display;
            }
            m_suggestModel->setStringList(titles);
            m_suggestCompleter->complete();   // 弹出候选
        });
}

void StationListWidget::onSuggestionPicked(const QString& display) {
    const auto it = m_suggestItems.constFind(display);
    if (it == m_suggestItems.constEnd()) return;

    const QJsonObject item = it.value();
    const QString title = item.value("title").toString();
    const double lat = item.value("lat").toDouble();
    const double lng = item.value("lng").toDouble();

    // 联想结果自带经纬度，直接定位查询，无需再走一次地址->经纬度转换
    m_suppressSuggest = true;
    m_addressEdit->setText(title);
    m_suppressSuggest = false;

    AppSession::instance().setPosition(lat, lng, title);
    m_latSpin->setValue(lat);
    m_lngSpin->setValue(lng);
    queryNearby(lat, lng);
}

void StationListWidget::queryNearby(double lat, double lng) {
    setStatus(QStringLiteral("正在查询附近充电站…"), true);

    QJsonObject data;
    data["lat"] = lat;
    data["lng"] = lng;

    // 优先走综合推荐（需求20：真实驾车距离/时长 + 价格 + 空闲率加权评分）；
    // 失败（如服务端外网不通）自动降级为普通附近查询
    NetClient::instance().sendRequest(Api::CmdStationRecommend, data,
        [this](const QJsonObject& resp, int code, const QString& /*msg*/) {
            if (code != 0) {
                queryNearbyFallback();
                return;
            }
            renderStations(resp.value("stations").toArray(),
                           resp.value("route_ok").toBool(true));
        });
}

void StationListWidget::queryNearbyFallback() {
    QJsonObject data;
    data["lat"] = AppSession::instance().latitude();
    data["lng"] = AppSession::instance().longitude();

    NetClient::instance().sendRequest(Api::CmdStationNearby, data,
        [this](const QJsonObject& resp, int code, const QString& msg) {
            if (code != 0) {
                setStatus(QStringLiteral("查询失败：%1").arg(msg), false);
                return;
            }
            renderStations(resp.value("stations").toArray(), true);
        });
}

void StationListWidget::renderStations(const QJsonArray& stations, bool routeOk) {
    m_listWidget->clear();
    if (stations.isEmpty()) {
        setStatus(QStringLiteral("附近暂无充电站"), false);
        return;
    }

    QString recoName, fastName;
    double fastMin = 0.0;
    for (const auto& v : stations) {
        const QJsonObject s = v.toObject();
        if (s.value("recommend").toBool()) recoName = s.value("name").toString();
        if (s.value("fastest").toBool()) {
            fastName = s.value("name").toString();
            fastMin = s.value("drive_min").toDouble();
        }
        addStationCard(s);
    }

    QString status = QStringLiteral("共找到 %1 座充电站").arg(stations.size());
    if (!recoName.isEmpty() && !fastName.isEmpty()) {
        status += QStringLiteral(" · 综合推荐：%1 · 最快到达：%2（约%3分钟）")
                      .arg(recoName, fastName)
                      .arg(fastMin, 0, 'f', 0);
    } else {
        status += QStringLiteral("（按距离由近及远）");
    }
    if (!routeOk) status += QStringLiteral(" · 实时路网暂不可用，驾车数据为估算");
    setStatus(status, true);
}

void StationListWidget::addStationCard(const QJsonObject& station) {
    auto* item = new QListWidgetItem(m_listWidget);
    item->setData(Qt::UserRole, station);   // 整卡点击进详情时取回站点信息
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

    // 需求20：综合推荐 / 最快到达 徽章
    const auto addBadge = [frame](QHBoxLayout* row, const QString& text, bool green) {
        auto* badge = new QLabel(text, frame);
        badge->setStyleSheet(green
            ? QStringLiteral("background: #e8f5e9; color: #2e7d32; border-radius: 8px;"
                             " padding: 1px 6px; font-size: 10px;")
            : QStringLiteral("background: #e3f2fd; color: #1565c0; border-radius: 8px;"
                             " padding: 1px 6px; font-size: 10px;"));
        row->addWidget(badge);
    };
    if (station.value("recommend").toBool())
        addBadge(topRow, QStringLiteral("综合推荐"), true);
    if (station.value("fastest").toBool())
        addBadge(topRow, QStringLiteral("最快到达"), false);
    topRow->addStretch(1);

    // 需求5：点击"距离"亦可发起导航（矩阵要求：点击距离或导航按钮）；
    // 推荐结果展示真实驾车距离/时长，普通查询展示直线距离
    const QString driveText = station.contains("drive_km")
        ? QStringLiteral("驾车 %1km · %2分钟")
              .arg(station.value("drive_km").toDouble(), 0, 'f', 1)
              .arg(station.value("drive_min").toDouble(), 0, 'f', 0)
        : QStringLiteral("%1 km").arg(station.value("distance").toDouble(), 0, 'f', 1);
    auto* dist = new QPushButton(driveText, frame);
    dist->setCursor(Qt::PointingHandCursor);
    dist->setStyleSheet(QStringLiteral(
        "QPushButton { color: #1976d2; font-weight: bold; border: none; background: transparent; }"
        "QPushButton:hover { text-decoration: underline; }"));
    connect(dist, &QPushButton::clicked, this,
            [this, station]() { requestDetailForNav(station); });
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
