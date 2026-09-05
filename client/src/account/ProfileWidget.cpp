#include "ProfileWidget.h"
#include "common/AppSession.h"
#include "common/NetClient.h"
#include "common/ApiDefs.h"

#include <QLabel>
#include <QLineEdit>
#include <QPushButton>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QFileDialog>
#include <QPixmap>
#include <QPainter>
#include <QBrush>
#include <QColor>
#include <QJsonObject>
#include <QMessageBox>

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

    m_hintLabel = new QLabel(this);
    m_hintLabel->setStyleSheet(QStringLiteral("color: #d9534f;"));
    m_hintLabel->setWordWrap(true);
    layout->addWidget(m_hintLabel);

    layout->addStretch(1);

    m_logoutBtn = new QPushButton(QStringLiteral("退出登录"), this);
    m_logoutBtn->setFixedHeight(40);
    layout->addWidget(m_logoutBtn);

    connect(m_changeAvatarBtn, &QPushButton::clicked, this, &ProfileWidget::onChooseAvatar);
    connect(m_saveBtn, &QPushButton::clicked, this, &ProfileWidget::onSaveProfile);
    connect(m_rechargeBtn, &QPushButton::clicked, this, &ProfileWidget::requestRecharge);
    connect(m_logoutBtn, &QPushButton::clicked, this, &ProfileWidget::onLogoutClicked);

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
    QPixmap pm;
    if (!avatar.isEmpty() && pm.load(avatar)) {
        setAvatarPixmap(pm);
    } else {
        setAvatarPixmap(defaultAvatarPixmap());
    }
    m_phoneLabel->setText(s.phone());
    m_nicknameEdit->setText(s.nickname());
    m_balanceLabel->setText(QStringLiteral("¥%1").arg(s.balance(), 0, 'f', 2));
    m_pendingAvatarPath.clear();
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
}

void ProfileWidget::onBalanceChanged(double balance) {
    m_balanceLabel->setText(QStringLiteral("¥%1").arg(balance, 0, 'f', 2));
}

void ProfileWidget::onChooseAvatar() {
    const QString path = QFileDialog::getOpenFileName(
        this, QStringLiteral("选择头像图片"), QString(),
        QStringLiteral("图片 (*.png *.jpg *.jpeg *.bmp)"));
    if (path.isEmpty()) return;

    QPixmap pm(path);
    if (pm.isNull()) {
        m_hintLabel->setText(QStringLiteral("图片加载失败"));
        return;
    }
    m_pendingAvatarPath = path;
    setAvatarPixmap(pm);

    // 选择即上传（协议 v1.0 avatar 为路径字符串；跨机显示受限见 README 说明）
    QJsonObject data;
    data["user_id"] = AppSession::instance().userId();
    data["avatar_url"] = path;
    NetClient::instance().sendRequest(Api::CmdUserUpdateProfile, data,
        [this](const QJsonObject& resp, int code, const QString& msg) {
            if (code == 0) {
                AppSession::instance().setAvatar(resp.value("avatar").toString());
                m_hintLabel->setStyleSheet(QStringLiteral("color: #2e7d32;"));
                m_hintLabel->setText(QStringLiteral("头像已更新"));
            } else {
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
