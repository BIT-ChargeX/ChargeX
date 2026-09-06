#pragma once
#include <QWidget>

class QLineEdit;
class QPushButton;
class QLabel;

// 账户模块-需求1：手机号+密码登录（首次登录自动注册）
// 命令：USER_LOGIN（携带 phone + password）
class LoginWidget : public QWidget {
    Q_OBJECT
public:
    explicit LoginWidget(QWidget* parent = nullptr);

    void setBusy(bool busy);

signals:
    void loginSucceeded();

private slots:
    void onLoginClicked();
    void onNetStateChanged(int state);

private:
    void requestLogin(const QString& phone, const QString& password);

    QLineEdit* m_phoneEdit;
    QLineEdit* m_passwordEdit;
    QPushButton* m_loginBtn;
    QLabel* m_hintLabel;
    QLabel* m_connLabel;
    bool m_busy = false;
};
