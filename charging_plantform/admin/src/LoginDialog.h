#pragma once
#include <QDialog>

class QLineEdit;
class QPushButton;
class QLabel;

// PC 管理端登录：经服务器 ADMIN_LOGIN 校验，成功后保存 token
class LoginDialog : public QDialog {
    Q_OBJECT
public:
    explicit LoginDialog(QWidget* parent = nullptr);

signals:
    void loginSucceeded();

private slots:
    void onLoginClicked();
    void onNetStateChanged(int state);

private:
    void setBusy(bool busy);

    QLineEdit* m_accountEdit;
    QLineEdit* m_passwordEdit;
    QPushButton* m_loginBtn;
    QLabel* m_connLabel;
    QLabel* m_hintLabel;
};
