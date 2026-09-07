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
#include <QMessageBox>

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

    auto* subTitle = new QLabel(QStringLiteral("手机号密码登录（未注册将自动创建账号）"), this);
    subTitle->setAlignment(Qt::AlignCenter);
    layout->addWidget(subTitle);

    m_connLabel = new QLabel(QStringLiteral("正在连接服务器…"), this);
    m_connLabel->setAlignment(Qt::AlignCenter);
    layout->addWidget(m_connLabel);

    // 手机号输入框：仅数字、最多 11 位
    m_phoneEdit = new QLineEdit(this);
    m_phoneEdit->setPlaceholderText(QStringLiteral("请输入11位手机号"));
    m_phoneEdit->setMaxLength(11);
    m_phoneEdit->setFixedHeight(38);
    layout->addWidget(m_phoneEdit);

    // 密码输入框：以圆点遮蔽显示
    m_passwordEdit = new QLineEdit(this);
    m_passwordEdit->setPlaceholderText(QStringLiteral("请输入密码"));
    m_passwordEdit->setEchoMode(QLineEdit::Password);
    m_passwordEdit->setMaxLength(32);
    m_passwordEdit->setFixedHeight(38);
    layout->addWidget(m_passwordEdit);

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

// 【需求1 - 登录/注册】点击"登录/注册"按钮：
// 1) 校验手机号格式（11 位、1 开头），不合法则弹提示并清空手机号，流程结束；
// 2) 校验密码非空，为空则弹提示；
// 3) 通过后携带手机号 + 密码请求服务端（由服务端判断登录还是自动注册）。
void LoginWidget::onLoginClicked() {
    if (m_busy) return;

    const QString phone = m_phoneEdit->text().trimmed();
    const QString password = m_passwordEdit->text();

    static const QRegularExpression re(QStringLiteral("^1[0-9]{10}$"));
    if (!re.match(phone).hasMatch()) {
        m_phoneEdit->clear();
        QMessageBox::warning(this, QStringLiteral("提示"),
                             QStringLiteral("手机号格式不正确，请重新输入"));
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
    requestLogin(phone, password);
}

// 发起登录请求并处理结果：
// 1) 密码错误 -> 服务端返回错误，客户端清空密码并弹提示；
// 2) 校验通过 -> 服务端已自动注册（若首次登录），客户端保存会话信息并进入主页。
void LoginWidget::requestLogin(const QString& phone, const QString& password) {
    setBusy(true);
    m_hintLabel->setText(QStringLiteral("登录中…"));

    QJsonObject data;
    data["phone"] = phone;
    data["password"] = password;

    NetClient::instance().sendRequest(Api::CmdUserLogin, data,
        [this, phone](const QJsonObject& resp, int code, const QString& msg) {
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

            AppSession::instance().setLogin(userId, phone, nickname, avatar, balance);
            emit loginSucceeded();
        });
}
