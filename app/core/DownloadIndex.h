#pragma once

#include <QDateTime>
#include <QHash>
#include <QString>

struct DownloadEntry {
    qint64 workId = 0;
    QString path;
    QString format; // "Epub" / "Fb2"
    QDateTime siteModifiedAt; // the work's lastModificationTime when we downloaded it
    qint64 size = 0;
    QDateTime downloadedAt;
    int chapterCount = 0; // chapters on the site when downloaded: drives the "+N гл." badge
};

// Which site books we put on the reader and where: drives "Читать", "Обновить" and "файл удалён".
class DownloadIndex {
public:
    explicit DownloadIndex(const QString &filePath);

    void put(const DownloadEntry &entry);
    bool contains(qint64 workId) const { return m_entries.contains(workId); }
    // Empty entry (workId == 0) when absent. No std::optional: the SDK ships libstdc++ 6.3.
    DownloadEntry get(qint64 workId) const { return m_entries.value(workId); }
    QList<DownloadEntry> all() const { return m_entries.values(); }
    void remove(qint64 workId);

    bool fileMissing(qint64 workId) const;
    bool hasUpdate(qint64 workId, const QDateTime &siteModifiedAt) const;

private:
    void save() const;

    QString m_path;
    QHash<qint64, DownloadEntry> m_entries;
};
