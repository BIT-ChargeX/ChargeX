#pragma once
#include <QObject>
#include <QString>
#include <QJsonObject>

// 管理端会话：保存 ADMIN_LOGIN 返回的 token/管理员信息，
// 管理命令统一由 attach() 注入 token。
class AdminSession : public QObject {
    Q_OBJECT
public:
    static AdminSession& instance();

    bool isLoggedIn() const;
    QString token() const;
    int adminId() const;
    QString adminName() const;

    // 给请求 data 注入 token（未登录时保持原样）
    void attach(QJsonObject& data) const;
    QJsonObject withToken(const QJsonObject& data) const;

public slots:
    void setLogin(int adminId, const QString& name, const QString& token);
    void logout();

signals:
    void loginChanged();
    void loggedOut();

private:
    explicit AdminSession(QObject* parent = nullptr);

    bool m_loggedIn = false;
    QString m_token;
    int m_adminId = 0;
    QString m_name;
};
