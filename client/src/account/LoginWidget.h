#pragma once
#include <QWidget>

class QLineEdit;
class QPushButton;
class QLabel;
class QTimer;

// 账户模块-需求1：手机号验证码登录（首次登录自动注册）
// 命令：USER_SEND_CODE（获取验证码）/ USER_LOGIN（验证码登录）
class LoginWidget : public QWidget {
    Q_OBJECT
public:
    explicit LoginWidget(QWidget* parent = nullptr);

    void setBusy(bool busy);

signals:
    void loginSucceeded();

private slots:
    void onSendCodeClicked();
    void onLoginClicked();
    void onNetStateChanged(int state);
    void onCountdownTick();

private:
    void requestLogin(const QString& phone, const QString& code);
    void startCountdown();

    QLineEdit* m_phoneEdit;
    QLineEdit* m_codeEdit;
    QPushButton* m_sendCodeBtn;
    QPushButton* m_loginBtn;
    QLabel* m_hintLabel;
    QLabel* m_connLabel;
    QTimer* m_countdownTimer;
    int m_countdown = 0;
    bool m_busy = false;
};
