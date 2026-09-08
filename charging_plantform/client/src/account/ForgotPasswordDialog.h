#pragma once
#include <QDialog>

class QLineEdit;
class QPushButton;
class QLabel;

// 忘记密码弹窗：输入邮箱 -> 发送验证码 -> 输入验证码 + 新密码 -> 重置
class ForgotPasswordDialog : public QDialog {
    Q_OBJECT
public:
    explicit ForgotPasswordDialog(QWidget* parent = nullptr);

private slots:
    void onSendCodeClicked();
    void onResetClicked();

private:
    void setBusy(bool busy);

    QLineEdit* m_emailEdit;
    QLineEdit* m_codeEdit;
    QLineEdit* m_passwordEdit;
    QLineEdit* m_confirmEdit;
    QPushButton* m_sendBtn;
    QPushButton* m_resetBtn;
    QLabel* m_hintLabel;
    bool m_busy = false;
};
