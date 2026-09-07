#include "MapPickerWidget.h"
#include "common/ApiDefs.h"

#include <QVBoxLayout>
#include <QLabel>
#include <QFile>
#include <QJsonDocument>
#include <QJsonObject>
#include <QUrl>
#include <QShowEvent>

#ifdef USE_QT_WEBENGINE
#include <QWebEngineView>
#include <QWebEnginePage>
#include <QWebChannel>
#endif

MapPickerWidget::MapPickerWidget(QWidget* parent) : QWidget(parent) {
    auto* layout = new QVBoxLayout(this);
    layout->setContentsMargins(0, 0, 0, 0);
    setMinimumHeight(220);

#ifdef USE_QT_WEBENGINE
    m_view = new QWebEngineView(this);
    m_bridge = new MapBridge(this);
    layout->addWidget(m_view);

    // 先注册桥接再加载页面，保证 qt.webChannelTransport 就绪
    auto* channel = new QWebChannel(m_view->page());
    m_view->page()->setWebChannel(channel);
    channel->registerObject(QStringLiteral("bridge"), m_bridge);

    connect(m_bridge, &MapBridge::picked, this, &MapPickerWidget::pointPicked);
    connect(m_bridge, &MapBridge::mapReady, this, [this]() {
        m_ready = true;
        applyCenter(m_lat, m_lng);
    });

    // 加载本地 HTML 模板，替换腾讯 key 占位符
    QFile f(QStringLiteral(":/resources/map_picker.html"));
    if (f.open(QIODevice::ReadOnly)) {
        QString html = QString::fromUtf8(f.readAll());
        html.replace(QStringLiteral("__TENCENT_KEY__"),
                     QString::fromLatin1(Api::kTencentMapKey));
        m_view->setHtml(html, QUrl(QStringLiteral("qrc:///resources/")));
    } else {
        m_view->setHtml(QStringLiteral(
            "<p style='color:#888;text-align:center;padding:40px 16px;'>地图资源加载失败</p>"));
    }
#else
    auto* label = new QLabel(
        QStringLiteral("当前构建未启用 QWebEngineWidgets，地图二次选点不可用"), this);
    label->setAlignment(Qt::AlignCenter);
    label->setStyleSheet(QStringLiteral("color: #888;"));
    layout->addWidget(label);
#endif
}

void MapPickerWidget::centerOn(double lat, double lng) {
    m_lat = lat;
    m_lng = lng;
    applyCenter(lat, lng);
}

// 页面可见后再次尝试创建地图（登录前容器尺寸为 0，JS 侧会暂存中心点）
void MapPickerWidget::showEvent(QShowEvent* event) {
    QWidget::showEvent(event);
    if (m_ready) applyCenter(m_lat, m_lng);
}

void MapPickerWidget::applyCenter(double lat, double lng) {
#ifdef USE_QT_WEBENGINE
    if (!m_view || !m_ready) return;   // 页面未就绪时暂存，就绪后统一应用
    const QString js = QStringLiteral("centerOn(%1, %2);")
                           .arg(lat, 0, 'f', 6)
                           .arg(lng, 0, 'f', 6);
    m_view->page()->runJavaScript(js);
#else
    Q_UNUSED(lat);
    Q_UNUSED(lng);
#endif
}

void MapPickerWidget::showStations(const QJsonArray& stations) {
#ifdef USE_QT_WEBENGINE
    if (!m_view || !m_ready) return;
    const QByteArray json = QJsonDocument(stations).toJson(QJsonDocument::Compact);
    m_view->page()->runJavaScript(
        QStringLiteral("setStations(%1);").arg(QString::fromUtf8(json)));
#else
    Q_UNUSED(stations);
#endif
}
