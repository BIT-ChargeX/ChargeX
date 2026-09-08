#pragma once
#include <QDialog>

class QLineEdit;
class QPushButton;
class QLabel;

// 注册弹窗：邮箱 + 邮箱验证码 + 设置密码 + 确认密码，注册成功后关闭弹窗返回登录
class RegisterDialog : public QDialog {
    Q_OBJECT
public:
    explicit RegisterDialog(QWidget* parent = nullptr);

private slots:
    void onSendCodeClicked();
    void onRegisterClicked();

private:
    void requestRegister(const QString& email, const QString& password, const QString& code);
    void setBusy(bool busy);

    QLineEdit* m_emailEdit;
    QLineEdit* m_codeEdit;
    QLineEdit* m_passwordEdit;
    QLineEdit* m_confirmEdit;
    QPushButton* m_sendBtn;
    QPushButton* m_registerBtn;
    QLabel* m_hintLabel;
    bool m_busy = false;
};
