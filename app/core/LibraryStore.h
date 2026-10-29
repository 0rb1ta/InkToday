#pragma once

#include "Models.h"

// The user's site library as last synced, persisted as JSON so it is visible offline.
class LibraryStore {
public:
    explicit LibraryStore(const QString &filePath);

    QList<WorkMeta> works() const { return m_works; }
    QDateTime lastSyncTime() const { return m_lastSync; }

    void replaceAll(const QList<WorkMeta> &works, const QDateTime &syncTime);
    void applyDiff(const QList<WorkMeta> &updated, const QList<qint64> &deleted, const QDateTime &syncTime);
    void setState(qint64 id, const QString &state); // "None" removes the book
    void clear();                                   // logout / another account

private:
    void save() const;

    QString m_path;
    QList<WorkMeta> m_works;
    QDateTime m_lastSync;
};
