#include "RegisterDialog.h"
#include "common/NetClient.h"
#include "common/ApiDefs.h"

#include <QLineEdit>
#include <QPushButton>
#include <QLabel>
#include <QHBoxLayout>
#include <QVBoxLayout>
#include <QRegularExpression>
#include <QJsonObject>
#include <QMessageBox>
#include <QFont>

RegisterDialog::RegisterDialog(QWidget* parent) : QDialog(parent) {
    setWindowTitle(QStringLiteral("注册新账号"));
    setModal(true);
    setFixedWidth(380);

    auto* layout = new QVBoxLayout(this);
    layout->setSpacing(12);

    auto* title = new QLabel(QStringLiteral("注册新账号"), this);
    QFont titleFont = title->font();
    titleFont.setPointSize(16);
    titleFont.setBold(true);
    title->setFont(titleFont);
    title->setAlignment(Qt::AlignCenter);
    layout->addWidget(title);

    m_emailEdit = new QLineEdit(this);
    m_emailEdit->setPlaceholderText(QStringLiteral("请输入邮箱"));
    m_emailEdit->setMaxLength(128);
    m_emailEdit->setFixedHeight(38);
    layout->addWidget(m_emailEdit);

    auto* codeRow = new QHBoxLayout;
    m_codeEdit = new QLineEdit(this);
    m_codeEdit->setPlaceholderText(QStringLiteral("6位验证码"));
    m_codeEdit->setMaxLength(6);
    m_codeEdit->setFixedHeight(38);
    codeRow->addWidget(m_codeEdit, 1);
    m_sendBtn = new QPushButton(QStringLiteral("发送验证码"), this);
    m_sendBtn->setFixedHeight(38);
    codeRow->addWidget(m_sendBtn);
    layout->addLayout(codeRow);

    m_passwordEdit = new QLineEdit(this);
    m_passwordEdit->setPlaceholderText(QStringLiteral("设置新密码（至少8位，含大小写字母和数字）"));
    m_passwordEdit->setEchoMode(QLineEdit::Password);
    m_passwordEdit->setMaxLength(32);
    m_passwordEdit->setFixedHeight(38);
    layout->addWidget(m_passwordEdit);

    m_confirmEdit = new QLineEdit(this);
    m_confirmEdit->setPlaceholderText(QStringLiteral("确认新密码"));
    m_confirmEdit->setEchoMode(QLineEdit::Password);
    m_confirmEdit->setMaxLength(32);
    m_confirmEdit->setFixedHeight(38);
    layout->addWidget(m_confirmEdit);

    m_hintLabel = new QLabel(this);
    m_hintLabel->setAlignment(Qt::AlignCenter);
    m_hintLabel->setStyleSheet(QStringLiteral("color: #d9534f;"));
    m_hintLabel->setWordWrap(true);
    layout->addWidget(m_hintLabel);

    m_registerBtn = new QPushButton(QStringLiteral("注册"), this);
    m_registerBtn->setFixedHeight(42);
    layout->addWidget(m_registerBtn);

    auto* cancelBtn = new QPushButton(QStringLiteral("取消"), this);
    cancelBtn->setFixedHeight(36);
    layout->addWidget(cancelBtn);

    connect(m_sendBtn, &QPushButton::clicked, this, &RegisterDialog::onSendCodeClicked);
    connect(m_registerBtn, &QPushButton::clicked, this, &RegisterDialog::onRegisterClicked);
    connect(cancelBtn, &QPushButton::clicked, this, &RegisterDialog::reject);
}

void RegisterDialog::setBusy(bool busy) {
    m_busy = busy;
    m_sendBtn->setEnabled(!busy);
    m_registerBtn->setEnabled(!busy);
    m_emailEdit->setEnabled(!busy);
    m_codeEdit->setEnabled(!busy);
    m_passwordEdit->setEnabled(!busy);
    m_confirmEdit->setEnabled(!busy);
}

void RegisterDialog::onSendCodeClicked() {
    if (m_busy) return;

    const QString email = m_emailEdit->text().trimmed();
    static const QRegularExpression re(QStringLiteral(
        "^[A-Za-z0-9._%+-]+@[A-Za-z0-9.-]+\\.[A-Za-z]{2,}$"));
    if (!re.match(email).hasMatch()) {
        m_emailEdit->clear();
        QMessageBox::warning(this, QStringLiteral("提示"), QStringLiteral("邮箱格式不正确"));
        return;
    }
    if (!NetClient::instance().isConnected()) {
        QMessageBox::warning(this, QStringLiteral("提示"), QStringLiteral("服务器未连接"));
        return;
    }

    setBusy(true);
    m_hintLabel->setStyleSheet(QStringLiteral("color: #d9534f;"));
    m_hintLabel->setText(QStringLiteral("发送中…"));

    QJsonObject data;
    data["email"] = email;
    data["purpose"] = QStringLiteral("register");
    NetClient::instance().sendRequest(Api::CmdUserSendCode, data,
        [this](const QJsonObject& resp, int code, const QString& msg) {
            setBusy(false);
            if (code != 0) {
                m_hintLabel->setText(QStringLiteral("发送失败：%1").arg(msg));
                return;
            }
            const bool demo = resp.value("demo").toBool();
            m_hintLabel->setStyleSheet(QStringLiteral("color: #2e7d32;"));
            m_hintLabel->setText(demo
                ? QStringLiteral("验证码已生成（演示模式，请查看服务端控制台日志）")
                : QStringLiteral("验证码已发送到邮箱，请注意查收"));
        });
}

void RegisterDialog::onRegisterClicked() {
    if (m_busy) return;

    const QString email = m_emailEdit->text().trimmed();
    const QString code = m_codeEdit->text().trimmed();
    const QString password = m_passwordEdit->text();
    const QString confirm = m_confirmEdit->text();

    static const QRegularExpression re(QStringLiteral(
        "^[A-Za-z0-9._%+-]+@[A-Za-z0-9.-]+\\.[A-Za-z]{2,}$"));
    if (!re.match(email).hasMatch()) {
        QMessageBox::warning(this, QStringLiteral("提示"), QStringLiteral("邮箱格式不正确"));
        return;
    }
    if (code.isEmpty()) {
        QMessageBox::warning(this, QStringLiteral("提示"), QStringLiteral("请输入邮箱验证码"));
        return;
    }
    if (password.length() < 8) {
        QMessageBox::warning(this, QStringLiteral("提示"), QStringLiteral("密码至少8位"));
        return;
    }
    bool hasUpper = false, hasLower = false, hasDigit = false;
    for (QChar c : password) {
        if (c.isUpper()) hasUpper = true;
        else if (c.isLower()) hasLower = true;
        else if (c.isDigit()) hasDigit = true;
    }
    if (!hasUpper || !hasLower || !hasDigit) {
        QMessageBox::warning(this, QStringLiteral("提示"),
                             QStringLiteral("密码需包含大写字母、小写字母和数字"));
        return;
    }
    if (password != confirm) {
        QMessageBox::warning(this, QStringLiteral("提示"), QStringLiteral("两次输入的密码不一致"));
        return;
    }

    setBusy(true);
    m_hintLabel->setStyleSheet(QStringLiteral("color: #d9534f;"));
    m_hintLabel->setText(QStringLiteral("注册中…"));

    requestRegister(email, password, code);
}

void RegisterDialog::requestRegister(const QString& email, const QString& password,
                                     const QString& code) {
    QJsonObject data;
    data["email"] = email;
    data["password"] = password;
    data["code"] = code;

    NetClient::instance().sendRequest(Api::CmdUserRegister, data,
        [this](const QJsonObject&, int code, const QString& msg) {
            setBusy(false);
            if (code != 0) {
                m_hintLabel->setText(QStringLiteral("注册失败：%1").arg(msg));
                return;
            }
            QMessageBox::information(this, QStringLiteral("提示"),
                                     QStringLiteral("注册成功，请登录"));
            accept();
        });
}
