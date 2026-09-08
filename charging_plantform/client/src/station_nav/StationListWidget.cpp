#include "StationListWidget.h"
#include "MapPickerWidget.h"
#include "common/NetClient.h"
#include "common/AppSession.h"
#include "common/MapApi.h"
#include "common/ApiDefs.h"

#include <QLineEdit>
#include <QPushButton>
#include <QComboBox>
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
    layout->setSpacing(6);

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

    // 地址输入 + 联想下拉：输入 >=2 字后 300ms 防抖自动调腾讯地点联想，
    // 也可以点"候选"按钮立即弹出；结果以"标题（区名）"展示
    auto* addrRow = new QHBoxLayout;
    m_addressEdit = new QLineEdit(this);
    m_addressEdit->setPlaceholderText(QStringLiteral("输入地址，如：北京理工大学"));
    m_suggestBtn = new QPushButton(QStringLiteral("候选 ▾"), this);
    m_locateAddrBtn = new QPushButton(QStringLiteral("定位"), this);
    addrRow->addWidget(m_addressEdit, 1);
    addrRow->addWidget(m_suggestBtn);
    addrRow->addWidget(m_locateAddrBtn);
    layout->addLayout(addrRow);

    // 当前定位经纬度展示
    m_posLabel = new QLabel(this);
    m_posLabel->setStyleSheet(QStringLiteral("color: #1565c0; font-size: 12px;"));
    m_posLabel->setWordWrap(true);
    layout->addWidget(m_posLabel);

    // 内嵌地图：选中联想地址后以其为中心；点击地图任意点 = 二次选点（最终经纬度）
    m_mapPicker = new MapPickerWidget(this);
    m_mapPicker->setFixedHeight(260);   // 固定高度，避免与列表抢空间导致跳动
    layout->addWidget(m_mapPicker);

    m_statusLabel = new QLabel(this);
    m_statusLabel->setWordWrap(true);
    layout->addWidget(m_statusLabel);

    m_listWidget = new QListWidget(this);
    m_listWidget->setSpacing(4);
    m_listWidget->setMinimumHeight(140);
    layout->addWidget(m_listWidget, 1);

    updatePosLabel(AppSession::instance().latitude(), AppSession::instance().longitude(),
                   QStringLiteral("北京市区默认"));

    // 联想下拉弹窗：独立 Tool 窗口（Qt::Popup 会被 WebEngine 原生窗口遮挡，
    // Tool 窗口始终浮在最上层，渲染可靠）
    m_suggestPopup = new QWidget(nullptr, Qt::Tool | Qt::FramelessWindowHint);
    m_suggestPopup->setAttribute(Qt::WA_ShowWithoutActivating);
    m_suggestPopup->setStyleSheet(QStringLiteral(
        "QWidget#suggestPopup { background: #ffffff; border: 1px solid #dde3ea; border-radius: 8px; }"));
    m_suggestPopup->setObjectName(QStringLiteral("suggestPopup"));
    auto* popupLayout = new QVBoxLayout(m_suggestPopup);
    popupLayout->setContentsMargins(2, 2, 2, 2);
    m_suggestList = new QListWidget(m_suggestPopup);
    m_suggestList->setFocusPolicy(Qt::NoFocus);
    m_suggestList->setStyleSheet(QStringLiteral(
        "QListWidget { border: none; background: #ffffff; }"
        "QListWidget::item { padding: 6px 4px; color: #22303c; }"
        "QListWidget::item:hover { background: #e6f9f0; }"
        "QListWidget::item:selected { background: #d9f5e8; color: #0f3d2e; }"));
    popupLayout->addWidget(m_suggestList);

    connect(m_locateAddrBtn, &QPushButton::clicked, this, &StationListWidget::onLocateByAddress);
    connect(m_suggestBtn, &QPushButton::clicked, this, &StationListWidget::onSuggestBtnClicked);
    connect(m_regionCombo, QOverload<int>::of(&QComboBox::currentIndexChanged),
            this, &StationListWidget::onRegionSelected);
    connect(m_mapPicker, &MapPickerWidget::pointPicked,
            this, &StationListWidget::onMapPicked);
    // 输入框失焦（点"定位"/点地图等）时收起下拉
    connect(m_addressEdit, &QLineEdit::editingFinished, this, [this]() {
        hideSuggestPopup();
    });

    m_suggestTimer = new QTimer(this);
    m_suggestTimer->setSingleShot(true);
    m_suggestTimer->setInterval(300);
    connect(m_suggestTimer, &QTimer::timeout, this, &StationListWidget::doSuggest);
    connect(m_addressEdit, &QLineEdit::textChanged, this, &StationListWidget::onAddressTextChanged);
    // 用 itemPressed：Tool 弹窗第一下点击被窗口激活吃掉，按下即选中只需单击
    connect(m_suggestList, &QListWidget::itemPressed,
            this, &StationListWidget::onSuggestionItemClicked);
    connect(m_addressEdit, &QLineEdit::returnPressed, this, [this]() {
        // 回车：有候选选第一个，没有候选走全文地址定位
        if (m_suggestPopup->isVisible() && m_suggestList->count() > 0) {
            onSuggestionPicked(m_suggestList->item(0)->text());
        } else {
            onLocateByAddress();
        }
    });

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

void StationListWidget::updatePosLabel(double lat, double lng, const QString& name) {
    m_posLabel->setText(QStringLiteral("📍 当前位置：%1, %2　（%3）")
                            .arg(lat, 0, 'f', 6)
                            .arg(lng, 0, 'f', 6)
                            .arg(name));
}

void StationListWidget::refreshNearby() {
    if (!AppSession::instance().isLoggedIn()) return;
    const double lat = AppSession::instance().latitude();
    const double lng = AppSession::instance().longitude();
    m_mapPicker->centerOn(lat, lng);
    queryNearby(lat, lng);
}

void StationListWidget::onLocateByAddress() {
    const QString address = m_addressEdit->text().trimmed();
    if (address.isEmpty()) {
        setStatus(QStringLiteral("请输入地址后再定位"), false);
        return;
    }

    m_locateAddrBtn->setEnabled(false);
    hideSuggestPopup();
    setStatus(QStringLiteral("正在调用腾讯地图定位…"), true);

    MapApi::instance().geocode(address, [this, address](bool ok, double lat, double lng,
                                                        int reliability, int deviation,
                                                        const QString& msg) {
        m_locateAddrBtn->setEnabled(true);
        if (!ok) {
            setStatus(QStringLiteral("定位失败：%1（可改输入更短的关键字，用下拉候选精确定位）").arg(msg), false);
            return;
        }
        AppSession::instance().setPosition(lat, lng, address);
        m_mapPicker->centerOn(lat, lng);
        updatePosLabel(lat, lng, address);
        queryNearby(lat, lng);

        // 匹配置信度低（reliability < 7 或偏差 >= 500 米）：
        // 提示偏差，并自动弹出联想候选供用户选择更精确的地点
        if (reliability < 7 || deviation >= 500) {
            setStatus(QStringLiteral("该地址匹配可能不够精确（偏差约 %1 米），"
                                     "可在下方下拉候选中选择更准确的地点").arg(deviation), false);
            doSuggest();
        }
    });
}

void StationListWidget::onSuggestBtnClicked() {
    if (m_addressEdit->text().trimmed().size() < 2) {
        setStatus(QStringLiteral("请先输入至少 2 个字（如：北京理工），再点候选"), false);
        return;
    }
    m_suggestTimer->stop();
    doSuggest();   // 立即请求并弹出候选
}

void StationListWidget::onRegionSelected(int index) {
    if (index < 0) return;
    const QString data = m_regionCombo->itemData(index).toString();
    const QStringList parts = data.split(QLatin1Char(','));
    if (parts.size() != 2) return;

    const double lat = parts[0].toDouble();
    const double lng = parts[1].toDouble();
    AppSession::instance().setPosition(lat, lng, m_regionCombo->currentText());
    m_mapPicker->centerOn(lat, lng);
    updatePosLabel(lat, lng, m_regionCombo->currentText());
    queryNearby(lat, lng);
}

void StationListWidget::onAddressTextChanged(const QString& text) {
    if (m_suppressSuggest) return;
    hideSuggestPopup();
    if (text.trimmed().size() < 2) {
        m_suggestTimer->stop();
        return;
    }
    m_suggestTimer->start();   // 防抖：停止输入 300ms 后才发请求
}

void StationListWidget::doSuggest() {
    const QString keyword = m_addressEdit->text().trimmed();
    if (keyword.size() < 2) return;

    // 联想范围固定为北京（与演示数据一致）；region 参数预留给 MapApi 扩展
    MapApi::instance().suggest(keyword, QStringLiteral("北京"),
        [this](bool ok, const QJsonArray& items, const QString& msg) {
            if (!ok) {
                hideSuggestPopup();
                setStatus(QStringLiteral("地址联想暂不可用：%1").arg(msg), false);
                return;
            }
            if (items.isEmpty()) {
                hideSuggestPopup();
                return;
            }
            QStringList titles;
            m_suggestItems.clear();
            for (const auto& v : items) {
                const QJsonObject it = v.toObject();
                const QString title = it.value("title").toString();
                const QString district = it.value("district").toString();
                const QString addr = it.value("address").toString();
                // 展示格式：北京理工大学中关村校区（海淀区）；区名为空时退回用地址区分
                const QString display = district.isEmpty()
                    ? (addr.isEmpty() ? title : QStringLiteral("%1（%2）").arg(title, addr))
                    : QStringLiteral("%1（%2）").arg(title, district);
                m_suggestItems.insert(display, it);
                titles << display;
            }
            showSuggestPopup(titles);
        });
}

void StationListWidget::showSuggestPopup(const QStringList& titles) {
    m_suggestList->clear();
    for (const QString& t : titles) m_suggestList->addItem(t);

    const int itemH = 30;
    const int h = qMin(titles.size(), 8) * itemH + 10;
    m_suggestPopup->setFixedSize(m_addressEdit->width(), h);
    m_suggestPopup->move(m_addressEdit->mapToGlobal(QPoint(0, m_addressEdit->height() + 2)));
    m_suggestPopup->show();
    m_suggestPopup->raise();
}

void StationListWidget::hideSuggestPopup() {
    m_suggestPopup->hide();
}

void StationListWidget::onSuggestionItemClicked(QListWidgetItem* item) {
    if (!item) return;
    onSuggestionPicked(item->text());
}

void StationListWidget::onSuggestionPicked(const QString& display) {
    hideSuggestPopup();
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
    m_mapPicker->centerOn(lat, lng);
    updatePosLabel(lat, lng, title);
    queryNearby(lat, lng);
}

// 地图二次选点：用户在地图上点击的位置作为最终经纬度
void StationListWidget::onMapPicked(double lat, double lng) {
    AppSession::instance().setPosition(lat, lng, QStringLiteral("地图选点"));
    updatePosLabel(lat, lng, QStringLiteral("地图选点"));
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
        m_mapPicker->showStations(QJsonArray());
        setStatus(QStringLiteral("附近暂无充电站"), false);
        return;
    }

    QString recoName;
    double recoMin = 0.0;
    QJsonArray markers;
    for (const auto& v : stations) {
        const QJsonObject s = v.toObject();
        if (s.value("recommend").toBool()) {
            recoName = s.value("name").toString();
            recoMin = s.value("drive_min").toDouble();
        }
        // 地图标记用字段
        QJsonObject m;
        m["lat"] = s.value("lat").toDouble();
        m["lng"] = s.value("lng").toDouble();
        m["name"] = s.value("name").toString();
        markers.append(m);
        addStationCard(s);
    }
    m_mapPicker->showStations(markers);

    // 服务端排序：综合推荐第一，其余按到达时间升序
    QString status = QStringLiteral("共找到 %1 座充电站").arg(stations.size());
    if (!recoName.isEmpty()) {
        status += QStringLiteral(" · 综合推荐：%1（驾车约%2分钟）")
                      .arg(recoName)
                      .arg(recoMin, 0, 'f', 0);
    } else {
        status += QStringLiteral("（按距离由近及远）");
    }
    if (!routeOk) status += QStringLiteral(" · 实时路网暂不可用，驾车数据为估算");
    setStatus(status, true);
}

void StationListWidget::addStationCard(const QJsonObject& station) {
    auto* item = new QListWidgetItem(m_listWidget);
    item->setData(Qt::UserRole, station);   // 整卡点击进详情时取回站点信息
    item->setSizeHint(QSize(0, 132));

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

    // 需求20：综合推荐徽章（第一位）
    if (station.value("recommend").toBool()) {
        auto* badge = new QLabel(QStringLiteral("综合推荐"), frame);
        badge->setStyleSheet(QStringLiteral(
            "background: #e8f5e9; color: #2e7d32; border-radius: 8px;"
            " padding: 1px 6px; font-size: 10px;"));
        topRow->addWidget(badge);
    }
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
        "QPushButton { color: #00b578; font-weight: bold; border: none; background: transparent; }"
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
    navBtn->setObjectName(QStringLiteral("secondaryBtn"));
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
