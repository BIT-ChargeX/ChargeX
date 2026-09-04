#include "NavWidget.h"
#include "common/AppSession.h"
#include "common/ApiDefs.h"

#include <QLineEdit>
#include <QComboBox>
#include <QPushButton>
#include <QLabel>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QUrlQuery>
#include <QDesktopServices>

#ifdef USE_QT_WEBENGINE
#include <QWebEngineView>
#endif

NavWidget::NavWidget(QWidget* parent) : QDialog(parent) {
    setWindowTitle(QStringLiteral("一键导航"));
    resize(430, 640);

    auto* layout = new QVBoxLayout(this);
    layout->setSpacing(10);

    auto* fromRow = new QHBoxLayout;
    fromRow->addWidget(new QLabel(QStringLiteral("起点"), this));
    m_fromEdit = new QLineEdit(this);
    m_fromEdit->setPlaceholderText(QStringLiteral("当前所在地址"));
    fromRow->addWidget(m_fromEdit, 1);
    layout->addLayout(fromRow);

    auto* toRow = new QHBoxLayout;
    toRow->addWidget(new QLabel(QStringLiteral("终点"), this));
    m_toEdit = new QLineEdit(this);
    m_toEdit->setReadOnly(true);
    toRow->addWidget(m_toEdit, 1);
    layout->addLayout(toRow);

    auto* modeRow = new QHBoxLayout;
    modeRow->addWidget(new QLabel(QStringLiteral("出行方式"), this));
    m_modeCombo = new QComboBox(this);
    m_modeCombo->addItem(QStringLiteral("驾车"), QStringLiteral("drive"));
    m_modeCombo->addItem(QStringLiteral("步行"), QStringLiteral("walk"));
    modeRow->addWidget(m_modeCombo, 1);
    layout->addLayout(modeRow);

    auto* btnRow = new QHBoxLayout;
    m_startBtn = new QPushButton(QStringLiteral("开始导航"), this);
    m_browserBtn = new QPushButton(QStringLiteral("在浏览器打开(备用)"), this);
    btnRow->addWidget(m_startBtn);
    btnRow->addWidget(m_browserBtn);
    layout->addLayout(btnRow);

    m_noteLabel = new QLabel(this);
    m_noteLabel->setWordWrap(true);
    m_noteLabel->setStyleSheet(QStringLiteral("color: #888;"));
    layout->addWidget(m_noteLabel);

#ifdef USE_QT_WEBENGINE
    m_view = new QWebEngineView(this);
    layout->addWidget(m_view, 1);
    connect(m_view, &QWebEngineView::urlChanged, this, [this](const QUrl&) {
        m_noteLabel->clear();
    });
#else
    m_noteLabel->setText(QStringLiteral("当前未启用 QWebEngineWidgets 模块，"
                                        "可点下方按钮在系统浏览器中打开腾讯地图路线。"));
#endif

    connect(m_startBtn, &QPushButton::clicked, this, &NavWidget::startRoute);
    connect(m_browserBtn, &QPushButton::clicked, this, [this]() {
        if (!m_routeUrl.isEmpty()) QDesktopServices::openUrl(m_routeUrl);
    });

    m_fromEdit->setText(AppSession::instance().address());
}

void NavWidget::setDestination(const QJsonObject& station) {
    m_toName = station.value("name").toString();
    m_toLat = station.value("lat").toDouble();
    m_toLng = station.value("lng").toDouble();
    m_toEdit->setText(m_toName);
    m_fromEdit->setText(AppSession::instance().address());
    if (m_toLat == 0.0 && m_toLng == 0.0) {
        m_noteLabel->setText(QStringLiteral("该站点缺少经纬度信息，无法规划路线。"));
    }
}

void NavWidget::buildRouteUrl() {
    m_routeUrl = QUrl();
    const QString key = QString(Api::kTencentMapKey).trimmed();
    if (key.isEmpty()) {
        m_noteLabel->setText(QStringLiteral("未配置腾讯地图 key，请在 ApiDefs.h 中填写 kTencentMapKey 后重试。"));
        return;
    }

    QUrlQuery q;
    q.addQueryItem(QStringLiteral("type"), m_modeCombo->currentData().toString());
    q.addQueryItem(QStringLiteral("from"), m_fromEdit->text().trimmed().isEmpty()
                                             ? AppSession::instance().address()
                                             : m_fromEdit->text().trimmed());
    q.addQueryItem(QStringLiteral("fromcoord"),
                   QStringLiteral("%1,%2").arg(AppSession::instance().latitude(), 0, 'f', 6)
                                           .arg(AppSession::instance().longitude(), 0, 'f', 6));
    q.addQueryItem(QStringLiteral("to"), m_toName);
    q.addQueryItem(QStringLiteral("tocoord"),
                   QStringLiteral("%1,%2").arg(m_toLat, 0, 'f', 6).arg(m_toLng, 0, 'f', 6));
    q.addQueryItem(QStringLiteral("referer"), QString(Api::kTencentMapReferer));
    q.addQueryItem(QStringLiteral("key"), key);

    QUrl url{QString(Api::kTencentRouteUrl)};
    url.setQuery(q);
    m_routeUrl = url;
}

void NavWidget::startRoute() {
    if (m_toLat == 0.0 && m_toLng == 0.0) return;
    buildRouteUrl();
    if (m_routeUrl.isEmpty()) return;
    loadIntoView();
}

void NavWidget::loadIntoView() {
#ifdef USE_QT_WEBENGINE
    if (m_view) {
        m_view->load(m_routeUrl);
        m_view->show();
        m_noteLabel->setText(QStringLiteral("正在加载腾讯地图路线规划…"));
        return;
    }
#endif
    m_noteLabel->setText(QStringLiteral("未启用内嵌浏览器，已生成路线地址，可点上方按钮在浏览器中打开。"));
}
