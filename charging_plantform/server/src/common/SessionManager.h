#pragma once
#include <QString>
#include <QHash>
#include <QReadWriteLock>
#include <QDateTime>

// 管理端会话：ADMIN_LOGIN 成功后签发随机 token，
// 后续 ADMIN/PILE/STATION/SALES 等管理命令需携带 token，由分发器校验。
// 线程安全：多个 TCP 工作线程并发访问。
class SessionManager {
public:
    static SessionManager& instance();

    // 创建会话并返回 token；同一管理员重新登录会作废旧 token
    QString createSession(int adminId, const QString& name);

    // 校验 token；成功返回管理员信息
    bool verify(const QString& token, int* adminId = nullptr,
                QString* name = nullptr) const;

    void removeSession(const QString& token);

private:
    struct Session {
        int adminId = 0;
        QString name;
        QDateTime createdAt;
    };

    SessionManager() = default;

    mutable QReadWriteLock m_lock;
    QHash<QString, Session> m_sessions;
};
