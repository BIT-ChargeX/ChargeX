#pragma once
#include <QString>

// MinIO (S3 兼容) 对象存储客户端。demo 采用公共桶直连（免签名），
// 上传/下载均走 HTTP；同步阻塞（内部走 httpSync），任意线程可调用。
class MinioClient {
public:
    // endpoint 如 http://localhost:9000，bucket 为桶名（如 avatars）
    static void configure(const QString& endpoint, const QString& bucket);

    // 上传对象（同步阻塞），成功返回 true 并通过 outUrl 回传公开访问 URL
    static bool upload(const QString& objectKey, const QByteArray& bytes,
                       const QString& contentType, QString* outUrl);

    // 构造对象的公开访问 URL
    static QString publicUrl(const QString& objectKey);

private:
    static QString s_endpoint;
    static QString s_bucket;
};
