#include "LoginWidget.h"
#include "common/NetClient.h"
#include "common/AppSession.h"
#include "common/ApiDefs.h"

#include <QLineEdit>
#include <QPushButton>
#include <QLabel>
#include <QVBoxLayout>
#include <QRegularExpression>
#include <QJsonObject>
#include <QFont>

LoginWidget::LoginWidget(QWidget* parent) : QWidget(parent) {
    auto* layout = new QVBoxLayout(this);
    layout->setSpacing(14);

    auto* title = new QLabel(QStringLiteral("东软充电 · 用户端"), this);
    title->setAlignment(Qt::AlignCenter);
    QFont titleFont = title->font();
    titleFont.setPointSize(18);
    titleFont.setBold(true);
    title->setFont(titleFont);
    layout->addWidget(title);

    auto* subTitle = new QLabel(QStringLiteral("手机号免密登录（未注册将自动创建账号）"), this);
    subTitle->setAlignment(Qt::AlignCenter);
    layout->addWidget(subTitle);

    m_connLabel = new QLabel(QStringLiteral("正在连接服务器…"), this);
    m_connLabel->setAlignment(Qt::AlignCenter);
    layout->addWidget(m_connLabel);

    m_phoneEdit = new QLineEdit(this);
    m_phoneEdit->setPlaceholderText(QStringLiteral("请输入11位手机号"));
    m_phoneEdit->setMaxLength(11);
    m_phoneEdit->setFixedHeight(38);
    layout->addWidget(m_phoneEdit);

    m_loginBtn = new QPushButton(QStringLiteral("登录 / 注册"), this);
    m_loginBtn->setFixedHeight(42);
    layout->addWidget(m_loginBtn);

    m_hintLabel = new QLabel(this);
    m_hintLabel->setAlignment(Qt::AlignCenter);
    m_hintLabel->setStyleSheet(QStringLiteral("color: #d9534f;"));
    m_hintLabel->setWordWrap(true);
    layout->addWidget(m_hintLabel);

    layout->addStretch(1);

    connect(m_loginBtn, &QPushButton::clicked, this, &LoginWidget::onLoginClicked);
    connect(&NetClient::instance(), &NetClient::stateChanged,
            this, &LoginWidget::onNetStateChanged);

    onNetStateChanged(static_cast<int>(NetClient::instance().state()));
}

void LoginWidget::setBusy(bool busy) {
    m_busy = busy;
    m_loginBtn->setEnabled(!busy);
    m_phoneEdit->setEnabled(!busy);
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

    const QString phone = m_phoneEdit->text().trimmed();
    static const QRegularExpression re(QStringLiteral("^1[0-9]{10}$"));
    if (!re.match(phone).hasMatch()) {
        m_hintLabel->setText(QStringLiteral("手机号格式不正确（需11位数字，1开头）"));
        return;
    }

    if (!NetClient::instance().isConnected()) {
        m_hintLabel->setText(QStringLiteral("服务器未连接，登录失败"));
        return;
    }

    m_hintLabel->clear();
    requestLogin(phone);
}

void LoginWidget::requestLogin(const QString& phone) {
    setBusy(true);
    m_hintLabel->setText(QStringLiteral("登录中…"));

    QJsonObject data;
    data["phone"] = phone;

    NetClient::instance().sendRequest(Api::CmdUserLogin, data,
        [this, phone](const QJsonObject& resp, int code, const QString& msg) {
            setBusy(false);
            if (code != 0) {
                m_hintLabel->setText(QStringLiteral("登录失败：%1").arg(msg));
                return;
            }

            int userId = resp.value("user_id").toInt();
            QString nickname = resp.value("nickname").toString();
            QString avatar = resp.value("avatar").toString();
            double balance = resp.value("balance").toDouble();

            AppSession::instance().setLogin(userId, phone, nickname, avatar, balance);
            emit loginSucceeded();
        });
}
