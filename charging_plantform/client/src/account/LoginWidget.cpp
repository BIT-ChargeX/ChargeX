#include "LoginWidget.h"
#include "common/NetClient.h"
#include "common/AppSession.h"
#include "common/ApiDefs.h"

#include <QLineEdit>
#include <QPushButton>
#include <QLabel>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QRegularExpression>
#include <QJsonObject>
#include <QFont>
#include <QMessageBox>
#include <QTimer>

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

    auto* subTitle = new QLabel(QStringLiteral("手机号验证码登录（未注册将自动创建账号）"), this);
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

    auto* codeRow = new QHBoxLayout;
    m_codeEdit = new QLineEdit(this);
    m_codeEdit->setPlaceholderText(QStringLiteral("请输入验证码"));
    m_codeEdit->setMaxLength(6);
    m_codeEdit->setFixedHeight(38);
    m_sendCodeBtn = new QPushButton(QStringLiteral("获取验证码"), this);
    m_sendCodeBtn->setFixedHeight(38);
    m_sendCodeBtn->setMinimumWidth(112);
    codeRow->addWidget(m_codeEdit, 1);
    codeRow->addWidget(m_sendCodeBtn);
    layout->addLayout(codeRow);

    m_loginBtn = new QPushButton(QStringLiteral("登录 / 注册"), this);
    m_loginBtn->setFixedHeight(42);
    layout->addWidget(m_loginBtn);

    m_hintLabel = new QLabel(this);
    m_hintLabel->setAlignment(Qt::AlignCenter);
    m_hintLabel->setStyleSheet(QStringLiteral("color: #d9534f;"));
    m_hintLabel->setWordWrap(true);
    layout->addWidget(m_hintLabel);

    layout->addStretch(1);

    m_countdownTimer = new QTimer(this);
    m_countdownTimer->setInterval(1000);
    connect(m_countdownTimer, &QTimer::timeout, this, &LoginWidget::onCountdownTick);

    connect(m_sendCodeBtn, &QPushButton::clicked, this, &LoginWidget::onSendCodeClicked);
    connect(m_loginBtn, &QPushButton::clicked, this, &LoginWidget::onLoginClicked);
    connect(&NetClient::instance(), &NetClient::stateChanged,
            this, &LoginWidget::onNetStateChanged);

    onNetStateChanged(static_cast<int>(NetClient::instance().state()));
}

void LoginWidget::setBusy(bool busy) {
    m_busy = busy;
    m_loginBtn->setEnabled(!busy);
    m_phoneEdit->setEnabled(!busy);
    m_codeEdit->setEnabled(!busy);
    if (m_countdown == 0) m_sendCodeBtn->setEnabled(!busy);
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

void LoginWidget::startCountdown() {
    m_countdown = 60;
    m_sendCodeBtn->setEnabled(false);
    onCountdownTick();
    m_countdownTimer->start();
}

void LoginWidget::onCountdownTick() {
    if (m_countdown > 0) {
        m_sendCodeBtn->setText(QStringLiteral("%1秒后重发").arg(m_countdown));
        --m_countdown;
    } else {
        m_countdownTimer->stop();
        m_sendCodeBtn->setEnabled(!m_busy);
        m_sendCodeBtn->setText(QStringLiteral("获取验证码"));
    }
}

void LoginWidget::onSendCodeClicked() {
    if (m_busy) return;

    const QString phone = m_phoneEdit->text().trimmed();
    static const QRegularExpression re(QStringLiteral("^1[0-9]{10}$"));
    if (!re.match(phone).hasMatch()) {
        m_phoneEdit->clear();
        QMessageBox::warning(this, QStringLiteral("提示"),
                             QStringLiteral("手机号格式不正确（需11位数字，1开头）"));
        return;
    }

    if (!NetClient::instance().isConnected()) {
        QMessageBox::warning(this, QStringLiteral("提示"),
                             QStringLiteral("服务器未连接，无法获取验证码"));
        return;
    }

    m_sendCodeBtn->setEnabled(false);   // 防止重复点击重复计费
    m_hintLabel->clear();

    QJsonObject data;
    data["phone"] = phone;
    NetClient::instance().sendRequest(Api::CmdUserSendCode, data,
        [this](const QJsonObject& resp, int code, const QString& msg) {
            if (code != 0) {
                m_sendCodeBtn->setEnabled(true);
                m_hintLabel->setText(QStringLiteral("获取验证码失败：%1").arg(msg));
                return;
            }
            // 演示模式：服务端未配置短信凭证时下发验证码，直接填入输入框
            const QString devCode = resp.value("dev_code").toString();
            if (!devCode.isEmpty()) {
                m_codeEdit->setText(devCode);
                m_hintLabel->setText(QStringLiteral("演示模式：验证码已自动填入"));
            } else {
                m_hintLabel->setText(QStringLiteral("验证码已发送，请注意查收短信"));
            }
            startCountdown();
        });
}

void LoginWidget::onLoginClicked() {
    if (m_busy) return;

    const QString phone = m_phoneEdit->text().trimmed();
    const QString code = m_codeEdit->text().trimmed();
    static const QRegularExpression re(QStringLiteral("^1[0-9]{10}$"));
    if (!re.match(phone).hasMatch()) {
        m_phoneEdit->clear();
        QMessageBox::warning(this, QStringLiteral("提示"),
                             QStringLiteral("手机号格式不正确，请重新输入"));
        return;
    }
    if (code.isEmpty()) {
        QMessageBox::warning(this, QStringLiteral("提示"),
                             QStringLiteral("请输入验证码"));
        return;
    }
    if (!NetClient::instance().isConnected()) {
        QMessageBox::warning(this, QStringLiteral("提示"),
                             QStringLiteral("服务器未连接，登录失败"));
        return;
    }

    m_hintLabel->clear();
    requestLogin(phone, code);
}

void LoginWidget::requestLogin(const QString& phone, const QString& code) {
    setBusy(true);
    m_hintLabel->setText(QStringLiteral("登录中…"));

    QJsonObject data;
    data["phone"] = phone;
    data["code"] = code;

    NetClient::instance().sendRequest(Api::CmdUserLogin, data,
        [this, phone](const QJsonObject& resp, int code, const QString& msg) {
            setBusy(false);
            if (code != 0) {
                m_codeEdit->clear();
                QMessageBox::warning(this, QStringLiteral("提示"),
                                     QStringLiteral("登录失败：%1").arg(msg));
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
