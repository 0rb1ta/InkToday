#pragma once

#include "Models.h"

#include <QDateTime>
#include <QHash>
#include <QObject>
#include <QQueue>
#include <QSet>

#include <functional>

class ApiClient;

// Work details (download permission, price, chapters...) are only available one book at a time
// (GET /v1/work/{id}/details). This cache keeps them on disk and fills gaps in the background,
// one request at a time, so tiles can show the right action button.
class StatusCache : public QObject {
    Q_OBJECT
public:
    StatusCache(ApiClient *client, const QString &filePath, std::function<QDateTime()> now, QObject *parent = nullptr);
    ~StatusCache() override;

    bool has(qint64 id) const { return m_entries.contains(id); }
    WorkDetails get(qint64 id) const { return m_entries.value(id).details; }

    // Queues ids whose details are missing or older than 24 h.
    void request(const QList<qint64> &ids);
    // The book on screen: ahead of everything queued (even if queued already).
    void requestNow(qint64 id);
    // Forces a refetch on the next request (e.g. after "Я оплатил").
    void invalidate(qint64 id);
    void clear(); // "Очистить кэш"
    // Brings Wi-Fi up before each request (the firmware turns it off); a "no" keeps the queue.
    void setNetworkGate(std::function<void(std::function<void(bool)>)> gate) { m_gate = std::move(gate); }

signals:
    void updated(qint64 id);
    void failed(qint64 id, const QString &message);

private:
    struct Entry {
        WorkDetails details;
        QDateTime fetchedAt;
    };

    bool isFresh(qint64 id) const;
    void pump();
    void fetchNext();
    void failUrgent(const QString &message);
    void save() const;
    void trim(); // keeps at most kMaxEntries, newest by fetchedAt
    void scheduleSave();

    ApiClient *m_client;
    QString m_path;
    std::function<QDateTime()> m_now;
    QHash<qint64, Entry> m_entries;
    QQueue<qint64> m_queue;
    QSet<qint64> m_queued;
    QSet<qint64> m_urgent; // asked for by requestNow (a card on screen)
    bool m_busy = false;
    std::function<void(std::function<void(bool)>)> m_gate;
    class QTimer *m_saveTimer;
};
