#pragma once

#include <QHash>
#include <QImage>
#include <QObject>

#include <functional>

class ApiClient;

// Book covers on disk (<dir>/<sha1(url)>.img), oldest files evicted beyond maxBytes.
// A failed or non-image download yields a null QImage; tiles draw a placeholder then.
class CoverCache : public QObject {
    Q_OBJECT
public:
    CoverCache(ApiClient *client, const QString &dir, qint64 maxBytes = 50 * 1024 * 1024, QObject *parent = nullptr);
    void get(const QString &url, std::function<void(const QImage &)> done);
    void clear();

private:
    QString fileFor(const QString &url) const;
    void evict();

    ApiClient *m_client;
    QString m_dir;
    qint64 m_maxBytes;
    QHash<QString, QList<std::function<void(const QImage &)>>> m_waiting; // url -> callbacks
    QHash<QString, QImage> m_memory; // recently shown covers, decoded
    QList<QString> m_memoryOrder;
    void remember(const QString &url, const QImage &img);
};
