#include "StatusCache.h"

#include "ApiClient.h"

#include <QFile>
#include <QJsonArray>
#include <QJsonDocument>
#include "FileIO.h"

#include <algorithm>
#include <QPointer>
#include <QTimer>

namespace {
const qint64 kFreshSecs = 24 * 3600;
// Browsing the catalog adds every book seen; the oldest answers go first.
const int kMaxEntries = 1500;
const int kTrimTo = 1300; // trim with slack: not a full sort on every new book once at the cap
}

StatusCache::StatusCache(ApiClient *client, const QString &filePath, std::function<QDateTime()> now, QObject *parent)
    : QObject(parent)
    , m_client(client)
    , m_path(filePath)
    , m_now(std::move(now))
    , m_saveTimer(new QTimer(this))
{
    // Writing the file after every book stalled the reader; batch writes instead.
    m_saveTimer->setSingleShot(true);
    m_saveTimer->setInterval(5000);
    connect(m_saveTimer, &QTimer::timeout, this, [this] { save(); });
    QFile f(m_path);
    if (!f.open(QIODevice::ReadOnly))
        return;
    for (const QJsonValue &v : QJsonDocument::fromJson(f.readAll()).array()) {
        const QJsonObject o = v.toObject();
        Entry e;
        e.details = WorkDetails::fromJson(o.value(QStringLiteral("details")).toObject());
        e.fetchedAt = QDateTime::fromString(o.value(QStringLiteral("fetchedAt")).toString(), Qt::ISODate);
        if (e.details.meta.id)
            m_entries.insert(e.details.meta.id, e);
    }
    trim();
}

void StatusCache::trim()
{
    if (m_entries.size() <= kMaxEntries)
        return;
    QList<QPair<QDateTime, qint64>> byAge;
    for (auto it = m_entries.constBegin(); it != m_entries.constEnd(); ++it)
        byAge << qMakePair(it->fetchedAt, it.key());
    std::sort(byAge.begin(), byAge.end());
    for (int i = 0; i < byAge.size() - kTrimTo; ++i)
        m_entries.remove(byAge[i].second);
}

bool StatusCache::isFresh(qint64 id) const
{
    auto it = m_entries.constFind(id);
    return it != m_entries.constEnd() && it->fetchedAt.isValid() && it->fetchedAt.secsTo(m_now()) < kFreshSecs;
}

void StatusCache::request(const QList<qint64> &ids)
{
    for (qint64 id : ids) {
        if (id <= 0 || isFresh(id) || m_queued.contains(id))
            continue;
        m_queue.enqueue(id);
        m_queued.insert(id);
    }
    pump();
}

void StatusCache::requestNow(qint64 id)
{
    if (id <= 0 || isFresh(id) || (m_queued.contains(id) && !m_queue.contains(id)))
        return; // fresh, or being fetched right now
    m_queue.removeAll(id);
    m_queue.prepend(id);
    m_queued.insert(id);
    m_urgent.insert(id);
    pump();
}

void StatusCache::failUrgent(const QString &message)
{
    // Only the book on screen hears about it (it offers «Повторить»).
    const QSet<qint64> urgent = m_urgent;
    m_urgent.clear();
    for (qint64 id : urgent) {
        m_queue.removeAll(id);
        m_queued.remove(id);
        emit failed(id, message);
    }
}

void StatusCache::invalidate(qint64 id)
{
    auto it = m_entries.find(id);
    if (it != m_entries.end())
        it->fetchedAt = QDateTime();
}

StatusCache::~StatusCache()
{
    if (m_saveTimer->isActive())
        save();
}

void StatusCache::scheduleSave()
{
    if (!m_saveTimer->isActive())
        m_saveTimer->start();
}

void StatusCache::clear()
{
    m_entries.clear();
    m_saveTimer->stop();
    save();
}

void StatusCache::pump()
{
    if (m_busy || m_queue.isEmpty())
        return;
    m_busy = true;
    if (m_gate) {
        QPointer<StatusCache> self(this);
        m_gate([self](bool online) {
            if (!self)
                return;
            if (online) {
                self->fetchNext();
            } else {
                self->m_busy = false; // background ids stay queued; the next request() tries again
                self->failUrgent(QStringLiteral("Нет подключения к интернету"));
            }
        });
        return;
    }
    fetchNext();
}

void StatusCache::fetchNext()
{
    if (m_queue.isEmpty()) {
        m_busy = false;
        return;
    }
    const qint64 id = m_queue.dequeue();
    m_client->get(QStringLiteral("/v1/work/%1/details").arg(id), [this, id](const QJsonDocument &doc, const ApiError *e) {
        m_busy = false;
        m_queued.remove(id);
        m_urgent.remove(id);
        if (!e) {
            m_entries.insert(id, Entry{WorkDetails::fromJson(doc.object()), m_now()});
            trim();
            scheduleSave();
            emit updated(id);
        } else {
            emit failed(id, e->message);
        }
        pump();
    });
}

void StatusCache::save() const
{
    QJsonArray arr;
    for (const Entry &e : m_entries) {
        arr.append(QJsonObject{{QStringLiteral("details"), e.details.toCompactJson()},
                               {QStringLiteral("fetchedAt"), e.fetchedAt.toUTC().toString(Qt::ISODate)}});
    }
    writeFileIfChanged(m_path, QJsonDocument(arr).toJson(QJsonDocument::Compact));
}
