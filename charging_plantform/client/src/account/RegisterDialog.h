#pragma once
#include <QDialog>

class QLineEdit;
class QPushButton;
class QLabel;

// 注册弹窗：邮箱 + 设置新密码 + 确认新密码，注册成功后关闭弹窗返回登录
class RegisterDialog : public QDialog {
    Q_OBJECT
public:
    explicit RegisterDialog(QWidget* parent = nullptr);

private slots:
    void onRegisterClicked();

private:
    void requestRegister(const QString& email, const QString& password);
    void setBusy(bool busy);

    QLineEdit* m_emailEdit;
    QLineEdit* m_passwordEdit;
    QLineEdit* m_confirmEdit;
    QPushButton* m_registerBtn;
    QLabel* m_hintLabel;
    bool m_busy = false;
};
