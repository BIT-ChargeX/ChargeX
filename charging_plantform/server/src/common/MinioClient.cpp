#include "MinioClient.h"
#include "HttpSync.h"

#include <QUrl>
#include <QDebug>

QString MinioClient::s_endpoint;
QString MinioClient::s_bucket;

void MinioClient::configure(const QString& endpoint, const QString& bucket) {
    s_endpoint = endpoint;
    s_bucket = bucket;
}

QString MinioClient::publicUrl(const QString& objectKey) {
    return QStringLiteral("%1/%2/%3").arg(s_endpoint, s_bucket, objectKey);
}

bool MinioClient::upload(const QString& objectKey, const QByteArray& bytes,
                         const QString& contentType, QString* outUrl) {
    if (s_endpoint.isEmpty() || objectKey.isEmpty()) return false;

    const QUrl url(publicUrl(objectKey));
    const QList<QPair<QByteArray, QByteArray>> headers = {
        { QByteArray("Content-Type"), contentType.toUtf8() },
    };

    const HttpSyncResult r = httpSync("PUT", url, bytes, headers);
    if (!r.ok) qWarning() << "[Minio] upload failed:" << r.body;
    if (r.ok && outUrl) *outUrl = publicUrl(objectKey);
    return r.ok;
}
