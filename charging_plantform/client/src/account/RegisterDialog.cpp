#include "RegisterDialog.h"
#include "common/NetClient.h"
#include "common/ApiDefs.h"

#include <QLineEdit>
#include <QPushButton>
#include <QLabel>
#include <QVBoxLayout>
#include <QRegularExpression>
#include <QJsonObject>
#include <QMessageBox>
#include <QFont>

RegisterDialog::RegisterDialog(QWidget* parent) : QDialog(parent) {
    setWindowTitle(QStringLiteral("注册新账号"));
    setModal(true);
    setFixedWidth(360);

    auto* layout = new QVBoxLayout(this);
    layout->setSpacing(12);

    auto* title = new QLabel(QStringLiteral("注册新账号"), this);
    QFont titleFont = title->font();
    titleFont.setPointSize(16);
    titleFont.setBold(true);
    title->setFont(titleFont);
    title->setAlignment(Qt::AlignCenter);
    layout->addWidget(title);

    m_phoneEdit = new QLineEdit(this);
    m_phoneEdit->setPlaceholderText(QStringLiteral("请输入11位手机号"));
    m_phoneEdit->setMaxLength(11);
    m_phoneEdit->setFixedHeight(38);
    layout->addWidget(m_phoneEdit);

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

    connect(m_registerBtn, &QPushButton::clicked, this, &RegisterDialog::onRegisterClicked);
    connect(cancelBtn, &QPushButton::clicked, this, &RegisterDialog::reject);
}

void RegisterDialog::setBusy(bool busy) {
    m_busy = busy;
    m_registerBtn->setEnabled(!busy);
    m_phoneEdit->setEnabled(!busy);
    m_passwordEdit->setEnabled(!busy);
    m_confirmEdit->setEnabled(!busy);
}

// 注册流程：
// 1) 校验手机号格式（11 位、1 开头、第二位 3-9）；
// 2) 校验密码强度（至少8位，含大写字母、小写字母和数字）；
// 3) 校验两次密码一致；
// 4) 通过后请求服务端注册（服务端拦截已注册的手机号）。
void RegisterDialog::onRegisterClicked() {
    if (m_busy) return;

    const QString phone = m_phoneEdit->text().trimmed();
    const QString password = m_passwordEdit->text();
    const QString confirm = m_confirmEdit->text();

    static const QRegularExpression phoneRe(QStringLiteral("^1[3-9][0-9]{9}$"));
    if (!phoneRe.match(phone).hasMatch()) {
        m_phoneEdit->clear();
        QMessageBox::warning(this, QStringLiteral("提示"),
                             QStringLiteral("手机号格式不正确，请输入正确的11位手机号"));
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
    if (!NetClient::instance().isConnected()) {
        QMessageBox::warning(this, QStringLiteral("提示"),
                             QStringLiteral("服务器未连接，注册失败"));
        return;
    }

    m_hintLabel->clear();
    requestRegister(phone, password);
}

void RegisterDialog::requestRegister(const QString& phone, const QString& password) {
    setBusy(true);
    m_hintLabel->setText(QStringLiteral("注册中…"));

    QJsonObject data;
    data["phone"] = phone;
    data["password"] = password;

    NetClient::instance().sendRequest(Api::CmdUserRegister, data,
        [this](const QJsonObject&, int code, const QString& msg) {
            setBusy(false);
            if (code != 0) {
                QMessageBox::warning(this, QStringLiteral("提示"),
                                     QStringLiteral("注册失败：%1").arg(msg));
                return;
            }
            QMessageBox::information(this, QStringLiteral("提示"),
                                     QStringLiteral("注册成功，请登录"));
            accept();
        });
}
