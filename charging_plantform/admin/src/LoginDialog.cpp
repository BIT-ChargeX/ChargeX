#include "LoginDialog.h"
#include "common/NetClient.h"
#include "common/AdminSession.h"
#include "common/ApiDefs.h"

#include <QLineEdit>
#include <QPushButton>
#include <QLabel>
#include <QFrame>
#include <QVBoxLayout>
#include <QHBoxLayout>

LoginDialog::LoginDialog(QWidget* parent) : QDialog(parent) {
    setWindowTitle(QStringLiteral("充电桩运营管理 - PC管理端登录"));
    setFixedSize(760, 430);

    auto* root = new QHBoxLayout(this);
    root->setContentsMargins(0, 0, 0, 0);
    root->setSpacing(0);

    auto* brand = new QFrame(this);
    brand->setObjectName(QStringLiteral("brandPanel"));
    brand->setFixedWidth(320);
    auto* bv = new QVBoxLayout(brand);
    bv->setContentsMargins(32, 36, 32, 36);
    bv->setSpacing(10);

    auto* title = new QLabel(QStringLiteral("充电桩运营\n管理后台"), brand);
    title->setObjectName(QStringLiteral("brandPanelTitle"));
    bv->addWidget(title);
    bv->addSpacing(4);
    auto* desc = new QLabel(QStringLiteral("PC 管理端\n业务均由服务器处理，\n本端仅通过协议远程操作。"), brand);
    desc->setObjectName(QStringLiteral("brandPanelDesc"));
    desc->setWordWrap(true);
    bv->addWidget(desc);
    bv->addStretch(1);
    auto* foot = new QLabel(QStringLiteral("远程接入 · Token 会话校验"), brand);
    foot->setObjectName(QStringLiteral("brandPanelSub"));
    bv->addWidget(foot);
    root->addWidget(brand);

    auto* form = new QWidget(this);
    auto* fv = new QVBoxLayout(form);
    fv->setContentsMargins(44, 40, 44, 36);
    fv->setSpacing(6);

    auto* formTitle = new QLabel(QStringLiteral("管理员登录"), form);
    formTitle->setObjectName(QStringLiteral("pageTitle"));
    fv->addWidget(formTitle);

    auto* sub = new QLabel(QStringLiteral("连接服务器并验证账号后进入"), form);
    sub->setObjectName(QStringLiteral("cardCaption"));
    fv->addWidget(sub);
    fv->addSpacing(14);

    m_connLabel = new QLabel(form);
    m_connLabel->setObjectName(QStringLiteral("hintMuted"));
    fv->addWidget(m_connLabel);

    auto* accLabel = new QLabel(QStringLiteral("账  号"), form);
    accLabel->setObjectName(QStringLiteral("fieldLabel"));
    fv->addWidget(accLabel);
    m_accountEdit = new QLineEdit(form);
    m_accountEdit->setPlaceholderText(QStringLiteral("管理员账号"));
    m_accountEdit->setFixedHeight(36);
    fv->addWidget(m_accountEdit);
    fv->addSpacing(6);

    auto* pwdLabel = new QLabel(QStringLiteral("密  码"), form);
    pwdLabel->setObjectName(QStringLiteral("fieldLabel"));
    fv->addWidget(pwdLabel);
    m_passwordEdit = new QLineEdit(form);
    m_passwordEdit->setPlaceholderText(QStringLiteral("密码"));
    m_passwordEdit->setEchoMode(QLineEdit::Password);
    m_passwordEdit->setFixedHeight(36);
    fv->addWidget(m_passwordEdit);
    fv->addSpacing(14);

    m_loginBtn = new QPushButton(QStringLiteral("登  录"), form);
    m_loginBtn->setObjectName(QStringLiteral("btnPrimary"));
    m_loginBtn->setFixedHeight(40);
    fv->addWidget(m_loginBtn);

    m_hintLabel = new QLabel(form);
    m_hintLabel->setWordWrap(true);
    m_hintLabel->setStyleSheet(QStringLiteral("color:#DC2626; font-size:12px;"));
    fv->addWidget(m_hintLabel);

    fv->addStretch(1);
    auto* tip = new QLabel(QStringLiteral("默认管理员：admin / 123456"), form);
    tip->setObjectName(QStringLiteral("hintMuted"));
    fv->addWidget(tip);
    root->addWidget(form, 1);

    connect(m_loginBtn, &QPushButton::clicked, this, &LoginDialog::onLoginClicked);
    connect(m_accountEdit, &QLineEdit::returnPressed, this, &LoginDialog::onLoginClicked);
    connect(m_passwordEdit, &QLineEdit::returnPressed, this, &LoginDialog::onLoginClicked);
    connect(&NetClient::instance(), &NetClient::stateChanged,
            this, &LoginDialog::onNetStateChanged);

    m_loginBtn->setCursor(Qt::PointingHandCursor);
    onNetStateChanged(static_cast<int>(NetClient::instance().state()));
}

void LoginDialog::setBusy(bool busy) {
    m_loginBtn->setEnabled(!busy);
    m_loginBtn->setText(busy ? QStringLiteral("验证中…") : QStringLiteral("登  录"));
}

void LoginDialog::onNetStateChanged(int state) {
    switch (state) {
    case static_cast<int>(NetClient::State::Connected):
        m_connLabel->setText(QStringLiteral("已连接服务器"));
        m_connLabel->setStyleSheet(QStringLiteral("color:#15803D;"));
        break;
    case static_cast<int>(NetClient::State::Connecting):
        m_connLabel->setText(QStringLiteral("正在连接服务器…"));
        m_connLabel->setStyleSheet(QStringLiteral("color:#475569;"));
        break;
    default:
        m_connLabel->setText(QStringLiteral("未连接服务器，请先启动 ChargingServer"));
        m_connLabel->setStyleSheet(QStringLiteral("color:#B91C1C;"));
        break;
    }
}

void LoginDialog::onLoginClicked() {
    if (!NetClient::instance().isConnected()) {
        m_hintLabel->setText(QStringLiteral("未连接服务器，无法登录"));
        return;
    }
    const QString account = m_accountEdit->text().trimmed();
    const QString password = m_passwordEdit->text();
    if (account.isEmpty() || password.isEmpty()) {
        m_hintLabel->setText(QStringLiteral("请输入账号与密码"));
        return;
    }

    setBusy(true);
    m_hintLabel->clear();
    QJsonObject data;
    data["account"] = account;
    data["password"] = password;

    NetClient::instance().sendRequest(Api::CmdAdminLogin, data,
        [this](const QJsonObject& resp, int code, const QString& msg) {
            setBusy(false);
            if (code != 0) {
                m_hintLabel->setText(QStringLiteral("登录失败：%1").arg(msg));
                return;
            }
            AdminSession::instance().setLogin(
                resp.value("admin_id").toInt(),
                resp.value("name").toString(),
                resp.value("token").toString());
            emit loginSucceeded();
            accept();   // 关闭登录框，main 中 exec() 返回 Accepted → 进入主界面
        });
}
