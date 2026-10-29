#include "LibrarySync.h"

#include "Log.h"

#include "ApiClient.h"
#include "LibraryStore.h"
#include "Models.h"

#include <QJsonArray>
#include <QUrlQuery>

namespace {
const int kPageSize = 100;
const int kMaxPages = 200; // guards against a server that never returns a short page
// lastSyncTime comes from the reader's clock, which may run ahead of the server's: anchor the
// next diff a little earlier so changes made in that gap are not skipped.
const qint64 kClockMarginSecs = 10 * 60;
}

LibrarySync::LibrarySync(ApiClient *client, LibraryStore *store, std::function<QDateTime()> now, QObject *parent)
    : QObject(parent)
    , m_client(client)
    , m_store(store)
    , m_now(std::move(now))
{
}

void LibrarySync::sync(std::function<void(const ApiError *)> done)
{
    const QDateTime startedAt = m_now().addSecs(-kClockMarginSecs);
    if (m_store->lastSyncTime().isValid())
        fetchDiff(startedAt, std::move(done));
    else
        fetchPage(1, {}, startedAt, std::move(done));
}

void LibrarySync::fetchPage(int page, QList<WorkMeta> collected, QDateTime startedAt,
                            std::function<void(const ApiError *)> done)
{
    const QString path = QStringLiteral("/v1/account/user-library?page=%1&pageSize=%2").arg(page).arg(kPageSize);
    const int epoch = m_epoch;
    m_client->get(path, [=](const QJsonDocument &doc, const ApiError *e) mutable {
        if (epoch != m_epoch)
            return; // cancelled: whoever cancelled has reset its own state
        if (e) {
            done(e);
            return;
        }
        const QJsonArray items = doc.object().value(QStringLiteral("worksInLibrary")).toArray();
        for (const QJsonValue &v : items)
            collected << WorkMeta::fromJson(v.toObject());
        if (items.size() < kPageSize || page >= kMaxPages) {
            m_store->replaceAll(collected, startedAt);
            done(nullptr);
            return;
        }
        fetchPage(page + 1, collected, startedAt, done);
    });
}

void LibrarySync::fetchDiff(QDateTime startedAt, std::function<void(const ApiError *)> done)
{
    QUrlQuery q;
    q.addQueryItem(QStringLiteral("lastSyncTime"), m_store->lastSyncTime().toUTC().toString(Qt::ISODate));
    const QString path = QStringLiteral("/v1/account/user-library-diff?") + q.toString(QUrl::FullyEncoded);
    const int epoch = m_epoch;
    m_client->get(path, [=](const QJsonDocument &doc, const ApiError *e) {
        if (epoch != m_epoch)
            return; // cancelled: whoever cancelled has reset its own state
        if (e && e->code == QLatin1String("InvalidLocalTime")) {
            // The reader's clock is ahead of the server: the full list does not depend on it.
            Log::write(QStringLiteral("library diff: InvalidLocalTime (reader clock ahead of server) -> full sync"));
            fetchPage(1, {}, startedAt, done);
            return;
        }
        if (e) {
            done(e);
            return;
        }
        const QJsonObject o = doc.object();
        QList<WorkMeta> updated;
        for (const QJsonValue &v : o.value(QStringLiteral("updatedWorksInLibrary")).toArray())
            updated << WorkMeta::fromJson(v.toObject());
        QList<qint64> deleted;
        for (const QJsonValue &v : o.value(QStringLiteral("deletedWorkIds")).toArray())
            deleted << qint64(v.toDouble());
        m_store->applyDiff(updated, deleted, startedAt);
        // Did the incremental view drift from the server (missed changes)? The diff never fills
        // totalCount (checked with a real account, 2026-10-29), but its shelf counters are right.
        int reading = 0, saved = 0, finished = 0;
        for (const WorkMeta &w : m_store->works()) {
            reading += w.libraryState == QLatin1String("Reading");
            saved += w.libraryState == QLatin1String("Saved");
            finished += w.libraryState == QLatin1String("Finished");
        }
        auto differs = [&o](const char *key, int local) {
            const QJsonValue v = o.value(QLatin1String(key));
            return v.isDouble() && v.toInt() != local;
        };
        if (differs("readingCount", reading) || differs("savedCount", saved) || differs("finishedCount", finished)) {
            Log::write(QStringLiteral("library diff: server shelves %1/%2/%3, local %4/%5/%6 -> full sync")
                           .arg(o.value(QStringLiteral("readingCount")).toInt())
                           .arg(o.value(QStringLiteral("savedCount")).toInt())
                           .arg(o.value(QStringLiteral("finishedCount")).toInt())
                           .arg(reading).arg(saved).arg(finished));
            fetchPage(1, {}, startedAt, done);
            return;
        }
        done(nullptr);
    });
}
