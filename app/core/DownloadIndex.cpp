#include "DownloadIndex.h"

#include <QFile>
#include <QFileInfo>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include "FileIO.h"

namespace {

QDateTime time(const QJsonObject &o, const char *key)
{
    return QDateTime::fromString(o.value(QLatin1String(key)).toString(), Qt::ISODate);
}

QString iso(const QDateTime &t)
{
    return t.isValid() ? t.toUTC().toString(Qt::ISODate) : QString();
}

} // namespace

DownloadIndex::DownloadIndex(const QString &filePath)
    : m_path(filePath)
{
    QFile f(m_path);
    if (!f.open(QIODevice::ReadOnly))
        return;
    for (const QJsonValue &v : QJsonDocument::fromJson(f.readAll()).array()) {
        const QJsonObject o = v.toObject();
        DownloadEntry e;
        e.workId = qint64(o.value(QStringLiteral("workId")).toDouble());
        e.path = o.value(QStringLiteral("path")).toString();
        e.format = o.value(QStringLiteral("format")).toString();
        e.siteModifiedAt = time(o, "siteModifiedAt");
        e.size = qint64(o.value(QStringLiteral("size")).toDouble());
        e.downloadedAt = time(o, "downloadedAt");
        e.chapterCount = o.value(QStringLiteral("chapterCount")).toInt();
        m_entries.insert(e.workId, e);
    }
}

void DownloadIndex::put(const DownloadEntry &entry)
{
    m_entries.insert(entry.workId, entry);
    save();
}

void DownloadIndex::remove(qint64 workId)
{
    m_entries.remove(workId);
    save();
}

bool DownloadIndex::fileMissing(qint64 workId) const
{
    return contains(workId) && !QFileInfo::exists(get(workId).path);
}

bool DownloadIndex::hasUpdate(qint64 workId, const QDateTime &siteModifiedAt) const
{
    const DownloadEntry e = get(workId);
    // Whole seconds: the API sends milliseconds, the saved index does not keep them.
    return contains(workId) && siteModifiedAt.isValid() && e.siteModifiedAt.isValid()
        && e.siteModifiedAt.secsTo(siteModifiedAt) > 0;
}

void DownloadIndex::save() const
{
    QJsonArray arr;
    for (const DownloadEntry &e : m_entries) {
        arr.append(QJsonObject{{QStringLiteral("workId"), double(e.workId)},
                               {QStringLiteral("path"), e.path},
                               {QStringLiteral("format"), e.format},
                               {QStringLiteral("siteModifiedAt"), iso(e.siteModifiedAt)},
                               {QStringLiteral("size"), double(e.size)},
                               {QStringLiteral("downloadedAt"), iso(e.downloadedAt)},
                               {QStringLiteral("chapterCount"), e.chapterCount}});
    }
    writeFileIfChanged(m_path, QJsonDocument(arr).toJson(QJsonDocument::Compact));
}
