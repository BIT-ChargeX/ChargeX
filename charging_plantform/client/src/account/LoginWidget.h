#pragma once
#include <QWidget>

class QLineEdit;
class QPushButton;
class QLabel;

// 账户模块-需求1：手机号免密登录（首次输入即自动注册）
// 负责人：肇子杰   命令：USER_LOGIN
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
    void requestLogin(const QString& phone);

    QLineEdit* m_phoneEdit;
    QPushButton* m_loginBtn;
    QLabel* m_hintLabel;
    QLabel* m_connLabel;
    bool m_busy = false;
};
