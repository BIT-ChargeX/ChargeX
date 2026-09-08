#include "LoginWidget.h"
#include "RegisterDialog.h"
#include "ForgotPasswordDialog.h"
#include "common/NetClient.h"
#include "common/AppSession.h"
#include "common/ApiDefs.h"

#include <QLineEdit>
#include <QPushButton>
#include <QLabel>
#include <QFrame>
#include <QVBoxLayout>
#include <QRegularExpression>
#include <QJsonObject>
#include <QFont>
#include <QMessageBox>

LoginWidget::LoginWidget(QWidget* parent) : QWidget(parent) {
    setObjectName(QStringLiteral("loginPage"));

    auto* layout = new QVBoxLayout(this);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->setSpacing(0);

    // 顶部品牌横幅
    auto* hero = new QLabel(QStringLiteral("⚡ ChargeX\n让绿色出行更简单"), this);
    hero->setObjectName(QStringLiteral("loginHero"));
    hero->setAlignment(Qt::AlignCenter);
    hero->setFixedHeight(180);
    layout->addWidget(hero);

    // 登录卡片
    auto* card = new QFrame(this);
    card->setObjectName(QStringLiteral("loginCard"));
    auto* cardLayout = new QVBoxLayout(card);
    cardLayout->setContentsMargins(28, 24, 28, 28);
    cardLayout->setSpacing(12);

    auto* body = new QVBoxLayout;
    body->setContentsMargins(20, 20, 20, 24);
    body->addWidget(card, 1);
    layout->addLayout(body, 1);

    auto* subTitle = new QLabel(QStringLiteral("欢迎回来，请登录"), card);
    subTitle->setObjectName(QStringLiteral("loginSubtitle"));
    subTitle->setAlignment(Qt::AlignCenter);
    cardLayout->addWidget(subTitle);

    m_connLabel = new QLabel(QStringLiteral("正在连接服务器…"), card);
    m_connLabel->setAlignment(Qt::AlignCenter);
    cardLayout->addWidget(m_connLabel);

    m_emailEdit = new QLineEdit(card);
    m_emailEdit->setPlaceholderText(QStringLiteral("请输入邮箱"));
    m_emailEdit->setMaxLength(128);
    m_emailEdit->setFixedHeight(40);
    cardLayout->addWidget(m_emailEdit);

    m_passwordEdit = new QLineEdit(card);
    m_passwordEdit->setPlaceholderText(QStringLiteral("请输入密码"));
    m_passwordEdit->setEchoMode(QLineEdit::Password);
    m_passwordEdit->setMaxLength(32);
    m_passwordEdit->setFixedHeight(40);
    cardLayout->addWidget(m_passwordEdit);

    m_loginBtn = new QPushButton(QStringLiteral("登  录"), card);
    m_loginBtn->setObjectName(QStringLiteral("primaryBtn"));
    m_loginBtn->setCursor(Qt::PointingHandCursor);
    m_loginBtn->setFixedHeight(44);
    cardLayout->addWidget(m_loginBtn);

    m_hintLabel = new QLabel(card);
    m_hintLabel->setAlignment(Qt::AlignCenter);
    m_hintLabel->setStyleSheet(QStringLiteral("color: #e5484d;"));
    m_hintLabel->setWordWrap(true);
    cardLayout->addWidget(m_hintLabel);

    m_registerBtn = new QPushButton(QStringLiteral("没有账号？立即注册"), card);
    m_registerBtn->setObjectName(QStringLiteral("linkBtn"));
    m_registerBtn->setFlat(true);
    m_registerBtn->setCursor(Qt::PointingHandCursor);
    cardLayout->addWidget(m_registerBtn);

    m_forgotBtn = new QPushButton(QStringLiteral("忘记密码？"), card);
    m_forgotBtn->setObjectName(QStringLiteral("linkBtnGray"));
    m_forgotBtn->setFlat(true);
    m_forgotBtn->setCursor(Qt::PointingHandCursor);
    cardLayout->addWidget(m_forgotBtn);

    cardLayout->addStretch(1);

    connect(m_loginBtn, &QPushButton::clicked, this, &LoginWidget::onLoginClicked);
    connect(m_registerBtn, &QPushButton::clicked, this, &LoginWidget::onRegisterClicked);
    connect(m_forgotBtn, &QPushButton::clicked, this, &LoginWidget::onForgotPasswordClicked);
    connect(&NetClient::instance(), &NetClient::stateChanged,
            this, &LoginWidget::onNetStateChanged);

    onNetStateChanged(static_cast<int>(NetClient::instance().state()));
}

void LoginWidget::setBusy(bool busy) {
    m_busy = busy;
    m_loginBtn->setEnabled(!busy);
    m_emailEdit->setEnabled(!busy);
    m_passwordEdit->setEnabled(!busy);
}

void LoginWidget::onNetStateChanged(int state) {
    switch (state) {
    case static_cast<int>(NetClient::State::Connected):
        m_connLabel->setText(QStringLiteral("已连接服务器"));
        m_connLabel->setStyleSheet(QStringLiteral("color: #2e7d32;"));
        break;
    case static_cast<int>(NetClient::State::Connecting):
        m_connLabel->setText(QStringLiteral("正在连接服务器…"));
        m_connLabel->setStyleSheet(QStringLiteral("color: #666;"));
        break;
    default:
        m_connLabel->setText(QStringLiteral("服务器未连接，请先启动服务端(127.0.0.1:9000)"));
        m_connLabel->setStyleSheet(QStringLiteral("color: #c62828;"));
        break;
    }
}

void LoginWidget::onLoginClicked() {
    if (m_busy) return;

    const QString email = m_emailEdit->text().trimmed();
    const QString password = m_passwordEdit->text();

    static const QRegularExpression re(QStringLiteral(
        "^[A-Za-z0-9._%+-]+@[A-Za-z0-9.-]+\\.[A-Za-z]{2,}$"));
    if (!re.match(email).hasMatch()) {
        m_emailEdit->clear();
        QMessageBox::warning(this, QStringLiteral("提示"),
                             QStringLiteral("邮箱格式不正确，请重新输入"));
        return;
    }
    if (password.isEmpty()) {
        QMessageBox::warning(this, QStringLiteral("提示"), QStringLiteral("请输入密码"));
        return;
    }
    if (!NetClient::instance().isConnected()) {
        QMessageBox::warning(this, QStringLiteral("提示"),
                             QStringLiteral("服务器未连接，登录失败"));
        return;
    }

    m_hintLabel->clear();
    requestLogin(email, password);
}

void LoginWidget::onRegisterClicked() {
    RegisterDialog dlg(this);
    dlg.exec();
}

void LoginWidget::onForgotPasswordClicked() {
    ForgotPasswordDialog dlg(this);
    dlg.exec();
}

void LoginWidget::requestLogin(const QString& email, const QString& password) {
    setBusy(true);
    m_hintLabel->setText(QStringLiteral("登录中…"));

    QJsonObject data;
    data["email"] = email;
    data["password"] = password;

    NetClient::instance().sendRequest(Api::CmdUserLogin, data,
        [this, email](const QJsonObject& resp, int code, const QString& msg) {
            setBusy(false);
            if (code != 0) {
                m_passwordEdit->clear();
                QMessageBox::warning(this, QStringLiteral("提示"),
                                     QStringLiteral("登录失败：%1").arg(msg));
                return;
            }

            int userId = resp.value("user_id").toInt();
            QString nickname = resp.value("nickname").toString();
            QString avatar = resp.value("avatar").toString();
            double balance = resp.value("balance").toDouble();

            AppSession::instance().setLogin(userId, email, nickname, avatar, balance);
            emit loginSucceeded();
        });
}
