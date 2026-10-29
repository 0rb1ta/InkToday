#include "LibraryStore.h"

#include <QFile>
#include <QJsonArray>
#include <QJsonDocument>
#include "FileIO.h"

LibraryStore::LibraryStore(const QString &filePath)
    : m_path(filePath)
{
    QFile f(m_path);
    if (!f.open(QIODevice::ReadOnly))
        return;
    const QJsonObject o = QJsonDocument::fromJson(f.readAll()).object();
    m_lastSync = QDateTime::fromString(o.value(QStringLiteral("lastSyncTime")).toString(), Qt::ISODate);
    for (const QJsonValue &v : o.value(QStringLiteral("works")).toArray())
        m_works << WorkMeta::fromJson(v.toObject());
}

void LibraryStore::replaceAll(const QList<WorkMeta> &works, const QDateTime &syncTime)
{
    m_works = works;
    m_lastSync = syncTime;
    save();
}

void LibraryStore::applyDiff(const QList<WorkMeta> &updated, const QList<qint64> &deleted, const QDateTime &syncTime)
{
    for (const WorkMeta &u : updated) {
        auto it = std::find_if(m_works.begin(), m_works.end(), [&](const WorkMeta &w) { return w.id == u.id; });
        if (it != m_works.end())
            *it = u;
        else
            m_works.prepend(u);
    }
    m_works.erase(std::remove_if(m_works.begin(), m_works.end(),
                                 [&](const WorkMeta &w) { return deleted.contains(w.id); }),
                  m_works.end());
    m_lastSync = syncTime;
    save();
}

void LibraryStore::setState(qint64 id, const QString &state)
{
    if (state == QLatin1String("None")) {
        m_works.erase(std::remove_if(m_works.begin(), m_works.end(), [&](const WorkMeta &w) { return w.id == id; }),
                      m_works.end());
    } else {
        for (WorkMeta &w : m_works)
            if (w.id == id)
                w.libraryState = state;
    }
    save();
}

void LibraryStore::clear()
{
    m_works.clear();
    m_lastSync = QDateTime();
    save();
}

void LibraryStore::save() const
{
    QJsonArray works;
    for (const WorkMeta &w : m_works)
        works.append(w.toJson());
    writeFileIfChanged(m_path, QJsonDocument(QJsonObject{{QStringLiteral("lastSyncTime"), m_lastSync.toUTC().toString(Qt::ISODate)},
                                                       {QStringLiteral("works"), works}})
                                   .toJson(QJsonDocument::Compact));
}
