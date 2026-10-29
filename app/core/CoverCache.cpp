#include "CoverCache.h"

#include "ApiClient.h"

#include <QCryptographicHash>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QSaveFile>

CoverCache::CoverCache(ApiClient *client, const QString &dir, qint64 maxBytes, QObject *parent)
    : QObject(parent)
    , m_client(client)
    , m_dir(dir)
    , m_maxBytes(maxBytes)
{
    QDir().mkpath(m_dir);
}

QString CoverCache::fileFor(const QString &url) const
{
    const QByteArray hash = QCryptographicHash::hash(url.toUtf8(), QCryptographicHash::Sha1).toHex();
    return m_dir + QLatin1Char('/') + QString::fromLatin1(hash) + QStringLiteral(".img");
}

void CoverCache::get(const QString &url, std::function<void(const QImage &)> done)
{
    if (m_memory.contains(url)) {
        done(m_memory.value(url));
        return;
    }
    const QString file = fileFor(url);
    if (QFileInfo::exists(file)) {
        const QImage img(file);
        remember(url, img);
        done(img);
        return;
    }
    auto &waiting = m_waiting[url];
    waiting << done;
    if (waiting.size() > 1)
        return; // a download for this url is already running

    m_client->getRaw(QUrl(url), [this, url, file](const HttpResponse &r, const ApiError *e) {
        QImage img;
        if (!e)
            img = QImage::fromData(r.body);
        if (!img.isNull()) {
            remember(url, img);
            QSaveFile f(file);
            if (f.open(QIODevice::WriteOnly)) {
                f.write(r.body);
                f.commit();
            }
            evict();
        }
        const auto callbacks = m_waiting.take(url);
        for (const auto &cb : callbacks)
            cb(img);
    });
}

void CoverCache::evict()
{
    QFileInfoList files = QDir(m_dir).entryInfoList({QStringLiteral("*.img")}, QDir::Files, QDir::Time | QDir::Reversed);
    qint64 total = 0;
    for (const QFileInfo &fi : files)
        total += fi.size();
    for (const QFileInfo &fi : files) { // oldest first
        if (total <= m_maxBytes)
            break;
        total -= fi.size();
        QFile::remove(fi.absoluteFilePath());
    }
}

void CoverCache::remember(const QString &url, const QImage &img)
{
    if (img.isNull())
        return;
    if (!m_memory.contains(url))
        m_memoryOrder << url;
    m_memory.insert(url, img);
    while (m_memoryOrder.size() > 48) // a few screens of tiles
        m_memory.remove(m_memoryOrder.takeFirst());
}

void CoverCache::clear()
{
    m_memory.clear();
    m_memoryOrder.clear();
    for (const QFileInfo &fi : QDir(m_dir).entryInfoList({QStringLiteral("*.img")}, QDir::Files))
        QFile::remove(fi.absoluteFilePath());
}
