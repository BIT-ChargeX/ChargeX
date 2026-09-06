#pragma once
#include <QString>
#include <QMap>
#include <QVariant>

// InfluxDB 2.x HTTP 客户端：line protocol 写入 + Flux 查询。
// 同步阻塞（内部走 httpSync），保证任意线程可调用，与 Service 同步返回一致。
class InfluxClient {
public:
    // 配置连接：url 如 http://localhost:8086，org/bucket/token 对应 InfluxDB 2.x 初始化参数
    static void configure(const QString& url, const QString& org,
                          const QString& bucket, const QString& token);

    // 写入一行或多行 line protocol（多行用 \n 分隔），返回是否成功
    static bool write(const QString& lineProtocol);

    // 便捷构造单行：measurement,tag=val field=val <ts_ms>
    static QString line(const QString& measurement,
                        const QMap<QString, QString>& tags,
                        const QMap<QString, QVariant>& fields,
                        qint64 timestampMs);

    // Flux 查询，返回原始响应体（annotated CSV）
    static QByteArray queryFlux(const QString& flux);

private:
    static QString s_url;
    static QString s_org;
    static QString s_bucket;
    static QString s_token;
};
