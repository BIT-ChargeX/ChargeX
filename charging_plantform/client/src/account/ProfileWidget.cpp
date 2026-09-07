#include "ProfileWidget.h"
#include "PointsWidget.h"
#include "charging/OrderListWidget.h"
#include "common/AppSession.h"
#include "common/NetClient.h"
#include "common/ApiDefs.h"

#include <QLabel>
#include <QLineEdit>
#include <QPushButton>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QGridLayout>
#include <QFileDialog>
#include <QFileInfo>
#include <QPixmap>
#include <QPainter>
#include <QBrush>
#include <QColor>
#include <QJsonObject>
#include <QMessageBox>
#include <QFile>
#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <QUrl>

namespace {

// 生成默认灰色头像
QPixmap defaultAvatarPixmap(int size = 96) {
    QPixmap pm(size, size);
    pm.fill(Qt::transparent);
    QPainter p(&pm);
    p.setRenderHint(QPainter::Antialiasing);
    p.setBrush(QBrush(QColor("#c9c9c9")));
    p.setPen(Qt::NoPen);
    p.drawEllipse(0, 0, size, size);
    p.setBrush(QBrush(QColor("#ffffff")));
    p.drawEllipse(size / 4, size / 6, size / 2, size / 2);   // 头
    p.drawEllipse(size / 6, size / 2 + size / 10, size * 2 / 3, size / 3); // 肩
    return pm;
}

} // namespace

ProfileWidget::ProfileWidget(QWidget* parent) : QWidget(parent) {
    auto* layout = new QVBoxLayout(this);
    layout->setSpacing(12);

    m_avatarLabel = new QLabel(this);
    m_avatarLabel->setAlignment(Qt::AlignCenter);
    m_avatarLabel->setFixedSize(120, 120);
    layout->addWidget(m_avatarLabel, 0, Qt::AlignCenter);

    auto* avatarRow = new QHBoxLayout;
    m_changeAvatarBtn = new QPushButton(QStringLiteral("更换头像"), this);
    avatarRow->addStretch(1);
    avatarRow->addWidget(m_changeAvatarBtn);
    avatarRow->addStretch(1);
    layout->addLayout(avatarRow);

    auto* nickRow = new QHBoxLayout;
    nickRow->addWidget(new QLabel(QStringLiteral("昵称"), this));
    m_nicknameEdit = new QLineEdit(this);
    m_nicknameEdit->setMaxLength(20);
    nickRow->addWidget(m_nicknameEdit, 1);
    m_saveBtn = new QPushButton(QStringLiteral("保存"), this);
    nickRow->addWidget(m_saveBtn);
    layout->addLayout(nickRow);

    auto* phoneRow = new QHBoxLayout;
    phoneRow->addWidget(new QLabel(QStringLiteral("手机号"), this));
    m_phoneLabel = new QLabel(this);
    phoneRow->addWidget(m_phoneLabel, 1);
    layout->addLayout(phoneRow);

    auto* balanceRow = new QHBoxLayout;
    balanceRow->addWidget(new QLabel(QStringLiteral("钱包余额"), this));
    m_balanceLabel = new QLabel(this);
    m_balanceLabel->setStyleSheet(QStringLiteral("font-size: 16px; color: #e65100; font-weight: bold;"));
    balanceRow->addWidget(m_balanceLabel, 1);
    m_rechargeBtn = new QPushButton(QStringLiteral("充值"), this);
    balanceRow->addWidget(m_rechargeBtn);
    layout->addLayout(balanceRow);

    // 环保足迹区（进入页面自动请求展示）
    auto* ecoTitle = new QLabel(QStringLiteral("环保足迹"), this);
    ecoTitle->setStyleSheet(QStringLiteral("font-weight: bold; margin-top: 8px;"));
    layout->addWidget(ecoTitle);

    auto* ecoGrid = new QGridLayout;
    ecoGrid->setColumnStretch(1, 1);
    ecoGrid->addWidget(new QLabel(QStringLiteral("累计充电量"), this), 0, 0);
    m_energyLabel = new QLabel(QStringLiteral("--"), this);
    ecoGrid->addWidget(m_energyLabel, 0, 1);
    ecoGrid->addWidget(new QLabel(QStringLiteral("累计减碳"), this), 1, 0);
    m_carbonLabel = new QLabel(QStringLiteral("--"), this);
    ecoGrid->addWidget(m_carbonLabel, 1, 1);
    ecoGrid->addWidget(new QLabel(QStringLiteral("等效植树"), this), 2, 0);
    m_treesLabel = new QLabel(QStringLiteral("--"), this);
    ecoGrid->addWidget(m_treesLabel, 2, 1);
    ecoGrid->addWidget(new QLabel(QStringLiteral("环保等级"), this), 3, 0);
    m_levelLabel = new QLabel(QStringLiteral("--"), this);
    ecoGrid->addWidget(m_levelLabel, 3, 1);
    layout->addLayout(ecoGrid);

    // 碳积分：可点击进入明细与兑换
    m_pointsBtn = new QPushButton(QStringLiteral("碳积分：-- 分  ›"), this);
    m_pointsBtn->setStyleSheet(QStringLiteral(
        "text-align: left; padding: 10px; color: #2e7d32; font-weight: bold;"));
    layout->addWidget(m_pointsBtn);

    // 我的充电订单：查看全部订单 + 结算未完成订单（从充电页迁入）
    m_ordersBtn = new QPushButton(QStringLiteral("我的充电订单  ›"), this);
    m_ordersBtn->setStyleSheet(QStringLiteral(
        "text-align: left; padding: 10px; color: #1565c0; font-weight: bold;"));
    layout->addWidget(m_ordersBtn);

    m_hintLabel = new QLabel(this);
    m_hintLabel->setStyleSheet(QStringLiteral("color: #d9534f;"));
    m_hintLabel->setWordWrap(true);
    layout->addWidget(m_hintLabel);

    layout->addStretch(1);

    m_logoutBtn = new QPushButton(QStringLiteral("退出登录"), this);
    m_logoutBtn->setFixedHeight(40);
    layout->addWidget(m_logoutBtn);

    m_pointsWidget = new PointsWidget(this);
    m_orderList = new OrderListWidget(this);

    connect(m_changeAvatarBtn, &QPushButton::clicked, this, &ProfileWidget::onChooseAvatar);
    connect(m_saveBtn, &QPushButton::clicked, this, &ProfileWidget::onSaveProfile);
    connect(m_rechargeBtn, &QPushButton::clicked, this, &ProfileWidget::requestRecharge);
    connect(m_logoutBtn, &QPushButton::clicked, this, &ProfileWidget::onLogoutClicked);
    connect(m_pointsBtn, &QPushButton::clicked, this, &ProfileWidget::onPointsClicked);
    connect(m_ordersBtn, &QPushButton::clicked, this, &ProfileWidget::onOrdersClicked);
    connect(m_orderList, &OrderListWidget::settleRequested, this, &ProfileWidget::settleRequested);

    connect(&AppSession::instance(), &AppSession::balanceChanged, this, &ProfileWidget::onBalanceChanged);
    connect(&AppSession::instance(), &AppSession::loginChanged, this, &ProfileWidget::onLoginChanged);
    connect(&AppSession::instance(), &AppSession::loggedOut, this, &ProfileWidget::onLoggedOut);
}

void ProfileWidget::setAvatarPixmap(const QPixmap& pm) {
    m_avatarLabel->setPixmap(pm.scaled(m_avatarLabel->size(),
                                       Qt::KeepAspectRatio, Qt::SmoothTransformation));
}

void ProfileWidget::applySession() {
    AppSession& s = AppSession::instance();

    const QString& avatar = s.avatar();
    if (avatar.startsWith(QStringLiteral("http://"))
        || avatar.startsWith(QStringLiteral("https://"))) {
        downloadAvatar(avatar);
    } else {
        QPixmap pm;
        if (!avatar.isEmpty() && pm.load(avatar)) {
            setAvatarPixmap(pm);
        } else {
            setAvatarPixmap(defaultAvatarPixmap());
        }
    }
    m_phoneLabel->setText(s.phone());
    m_nicknameEdit->setText(s.nickname());
    m_balanceLabel->setText(QStringLiteral("¥%1").arg(s.balance(), 0, 'f', 2));
    m_pendingAvatarPath.clear();
}

void ProfileWidget::downloadAvatar(const QString& url) {
    auto* nam = new QNetworkAccessManager(this);
    QNetworkReply* reply = nam->get(QNetworkRequest(QUrl(url)));
    connect(reply, &QNetworkReply::finished, this, [this, reply]() {
        QPixmap pm;
        if (reply->error() == QNetworkReply::NoError && pm.loadFromData(reply->readAll())) {
            setAvatarPixmap(pm);
        } else {
            setAvatarPixmap(defaultAvatarPixmap());
        }
        reply->deleteLater();
    });
}

void ProfileWidget::onLoginChanged() {
    if (AppSession::instance().isLoggedIn()) applySession();
}

void ProfileWidget::onLoggedOut() {
    setAvatarPixmap(defaultAvatarPixmap());
    m_phoneLabel->clear();
    m_nicknameEdit->clear();
    m_balanceLabel->setText(QStringLiteral("¥0.00"));
    m_hintLabel->clear();
    m_pendingAvatarPath.clear();
    resetEcoFootprint();
}

void ProfileWidget::resetEcoFootprint() {
    m_energyLabel->setText(QStringLiteral("--"));
    m_carbonLabel->setText(QStringLiteral("--"));
    m_treesLabel->setText(QStringLiteral("--"));
    m_levelLabel->setText(QStringLiteral("--"));
    m_pointsBtn->setText(QStringLiteral("碳积分：-- 分  ›"));
}

void ProfileWidget::onPointsClicked() {
    if (!AppSession::instance().isLoggedIn()) return;
    m_pointsWidget->refresh();
    m_pointsWidget->show();
    m_pointsWidget->raise();
    m_pointsWidget->activateWindow();
}

void ProfileWidget::onOrdersClicked() {
    if (!AppSession::instance().isLoggedIn()) return;
    m_orderList->refresh();
    m_orderList->show();
    m_orderList->raise();
    m_orderList->activateWindow();
}

void ProfileWidget::refreshOrders() {
    if (AppSession::instance().isLoggedIn()) m_orderList->refresh();
}

void ProfileWidget::refresh() {
    if (!AppSession::instance().isLoggedIn()) return;

    QJsonObject data;
    data["user_id"] = AppSession::instance().userId();
    NetClient::instance().sendRequest(Api::CmdUserGetBalance, data,
        [](const QJsonObject& resp, int code, const QString& /*msg*/) {
            if (code == 0) {
                AppSession::instance().setBalance(resp.value("balance").toDouble());
            }
        });

    // 进入页面自动请求碳积分与环保足迹
    NetClient::instance().sendRequest(Api::CmdUserCarbonStats, data,
        [this](const QJsonObject& resp, int code, const QString& /*msg*/) {
            if (code != 0) return;
            const double energy = resp.value("energy_kwh").toDouble();
            const double carbon = resp.value("carbon_kg").toDouble();
            const double trees  = resp.value("trees").toDouble();
            const int points    = resp.value("points").toInt();
            const QString level = resp.value("level").toString();

            m_energyLabel->setText(QStringLiteral("%1 kWh").arg(energy, 0, 'f', 1));
            m_carbonLabel->setText(QStringLiteral("%1 kg CO₂").arg(carbon, 0, 'f', 1));
            m_treesLabel->setText(QStringLiteral("%1 棵").arg(trees, 0, 'f', 2));
            m_levelLabel->setText(level);
            m_pointsBtn->setText(QStringLiteral("碳积分：%1 分  ›").arg(points));
        });
}

void ProfileWidget::onBalanceChanged(double balance) {
    m_balanceLabel->setText(QStringLiteral("¥%1").arg(balance, 0, 'f', 2));
}

void ProfileWidget::onChooseAvatar() {
    const QString path = QFileDialog::getOpenFileName(
        this, QStringLiteral("选择头像图片"), QString(),
        QStringLiteral("图片 (*.png *.jpg *.jpeg)"));
    if (path.isEmpty()) return;

    // 校验图片格式（jpg/jpeg/png），不合法则保持原头像不变
    const QString suffix = QFileInfo(path).suffix().toLower();
    if (suffix != QStringLiteral("png")
        && suffix != QStringLiteral("jpg")
        && suffix != QStringLiteral("jpeg")) {
        m_hintLabel->setStyleSheet(QStringLiteral("color: #d9534f;"));
        m_hintLabel->setText(QStringLiteral("头像格式不支持，请选择 jpg/png 图片"));
        return;
    }

    // 校验图片大小（≤ 2MB），不合法则保持原头像不变
    const qint64 kMaxBytes = 2 * 1024 * 1024;
    const qint64 sizeBytes = QFileInfo(path).size();
    if (sizeBytes <= 0 || sizeBytes > kMaxBytes) {
        m_hintLabel->setStyleSheet(QStringLiteral("color: #d9534f;"));
        m_hintLabel->setText(QStringLiteral("头像图片过大，请选择 2MB 以内的图片"));
        return;
    }

    QPixmap pm(path);
    if (pm.isNull()) {
        m_hintLabel->setText(QStringLiteral("图片加载失败"));
        return;
    }
    m_pendingAvatarPath = path;
    setAvatarPixmap(pm);

    // 读取文件字节 -> base64 -> 上传 MinIO 对象存储，换取跨设备可访问的公开 URL
    QFile file(path);
    if (!file.open(QIODevice::ReadOnly)) {
        m_hintLabel->setText(QStringLiteral("图片读取失败"));
        return;
    }
    const QByteArray bytes = file.readAll();
    file.close();

    QJsonObject data;
    data["user_id"] = AppSession::instance().userId();
    data["file_name"] = QFileInfo(path).fileName();
    data["data_b64"] = QString::fromLatin1(bytes.toBase64());

    NetClient::instance().sendRequest(Api::CmdAvatarUpload, data,
        [this](const QJsonObject& resp, int code, const QString& msg) {
            if (code == 0) {
                AppSession::instance().setAvatar(resp.value("avatar").toString());
                m_hintLabel->setStyleSheet(QStringLiteral("color: #2e7d32;"));
                m_hintLabel->setText(QStringLiteral("头像已上传"));
            } else {
                m_hintLabel->setStyleSheet(QStringLiteral("color: #d9534f;"));
                m_hintLabel->setText(QStringLiteral("头像上传失败：%1").arg(msg));
            }
        });
}

void ProfileWidget::onSaveProfile() {
    const QString nickname = m_nicknameEdit->text().trimmed();
    if (nickname.isEmpty()) {
        m_hintLabel->setText(QStringLiteral("昵称不能为空"));
        return;
    }

    QJsonObject data;
    data["user_id"] = AppSession::instance().userId();
    data["nickname"] = nickname;

    NetClient::instance().sendRequest(Api::CmdUserUpdateProfile, data,
        [this](const QJsonObject& resp, int code, const QString& msg) {
            if (code == 0) {
                AppSession::instance().setNickname(resp.value("nickname").toString());
                m_hintLabel->setStyleSheet(QStringLiteral("color: #2e7d32;"));
                m_hintLabel->setText(QStringLiteral("昵称已保存"));
            } else {
                m_hintLabel->setText(QStringLiteral("保存失败：%1").arg(msg));
            }
        });
}

void ProfileWidget::onLogoutClicked() {
    if (QMessageBox::question(this, QStringLiteral("退出登录"),
                              QStringLiteral("确定要退出当前账号吗？"))
        != QMessageBox::Yes) {
        return;
    }
    AppSession::instance().logout();
    emit logoutRequested();
}
