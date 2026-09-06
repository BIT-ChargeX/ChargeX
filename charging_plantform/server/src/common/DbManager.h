#pragma once
#include <QString>
#include <QSqlDatabase>

// 数据库管理：SQLite 单文件，负责连接初始化、建表、演示数据播种。
// 主线程(GUI)与每个 TCP 连接线程都使用各自独立的连接（SQLite 连接不能跨线程共享）。
class DbManager {
public:
    // 在主线程调用一次：设置库文件路径并完成建表/初始化
    static void init(const QString& dbPath);

    // 每个线程拿自己独立的连接（懒加载），可直接用于 QSqlQuery
    static QSqlDatabase threadDb();

    static QString dbPath();

private:
    static void createSchema(QSqlDatabase db);
    static void ensurePileRealtimeColumns(QSqlDatabase db);
    static void seedDemo(QSqlDatabase db);

    static QString s_path;
};
