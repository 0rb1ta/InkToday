#pragma once

#include <QDateTime>
#include <QObject>

#include <functional>

class ApiClient;
class LibraryStore;
struct ApiError;
struct WorkMeta;

// Pulls the user's library: everything on first run, then only changes since the last sync.
class LibrarySync : public QObject {
    Q_OBJECT
public:
    LibrarySync(ApiClient *client, LibraryStore *store, std::function<QDateTime()> now, QObject *parent = nullptr);
    void sync(std::function<void(const ApiError *)> done);
    // Results of a sync already in flight are dropped and its callback never runs (logout /
    // account switch): the caller resets its own "syncing" state.
    void cancel() { ++m_epoch; }

private:
    void fetchPage(int page, QList<WorkMeta> collected, QDateTime startedAt, std::function<void(const ApiError *)> done);
    void fetchDiff(QDateTime startedAt, std::function<void(const ApiError *)> done);

    ApiClient *m_client;
    LibraryStore *m_store;
    std::function<QDateTime()> m_now;
    int m_epoch = 0;
};
