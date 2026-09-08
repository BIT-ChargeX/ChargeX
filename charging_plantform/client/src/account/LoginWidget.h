#pragma once
#include <QWidget>

class QLineEdit;
class QPushButton;
class QLabel;

// 账户模块-需求1：邮箱+密码登录；注册/忘记密码通过弹窗完成
// 命令：USER_LOGIN（email + password）、USER_REGISTER、USER_SEND_CODE、USER_RESET_PASSWORD
class LoginWidget : public QWidget {
    Q_OBJECT
public:
    explicit LoginWidget(QWidget* parent = nullptr);

    void setBusy(bool busy);

signals:
    void loginSucceeded();

private slots:
    void onLoginClicked();
    void onRegisterClicked();
    void onForgotPasswordClicked();
    void onNetStateChanged(int state);

private:
    void requestLogin(const QString& email, const QString& password);

    QLineEdit* m_emailEdit;
    QLineEdit* m_passwordEdit;
    QPushButton* m_loginBtn;
    QPushButton* m_registerBtn;
    QPushButton* m_forgotBtn;
    QLabel* m_hintLabel;
    QLabel* m_connLabel;
    bool m_busy = false;
};
