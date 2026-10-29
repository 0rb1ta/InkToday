#include "RateLimiter.h"

#include <QRegularExpression>

namespace {
const qint64 kWindowMs = 60000;
}

RateLimiter::RateLimiter(int perMinute)
    : m_perMinute(perMinute)
{
}

QString RateLimiter::endpointKey(const QString &pathAndQuery)
{
    static const QRegularExpression numericSegment(QStringLiteral("/\\d+(?=/|$)"));
    QString path = pathAndQuery.section(QLatin1Char('?'), 0, 0);
    path.replace(numericSegment, QStringLiteral("/{id}"));
    return path;
}

qint64 RateLimiter::delayMs(const QString &key, qint64 nowMs) const
{
    qint64 delay = qMax<qint64>(0, m_blockedUntil.value(key, 0) - nowMs);
    const QQueue<qint64> sent = m_sent.value(key);
    auto windowDelay = [&](int count, qint64 window) {
        int inWindow = 0;
        qint64 oldest = nowMs;
        for (qint64 t : sent) {
            if (nowMs - t < window) {
                ++inWindow;
                oldest = qMin(oldest, t);
            }
        }
        // The oldest request still inside the window decides when a slot frees up.
        return inWindow >= count ? oldest + window - nowMs : qint64(0);
    };
    delay = qMax(delay, windowDelay(m_perMinute, kWindowMs));
    if (m_learned.contains(key)) {
        const auto limit = m_learned.value(key);
        delay = qMax(delay, windowDelay(limit.first, limit.second));
    }
    return delay;
}

void RateLimiter::record(const QString &key, qint64 nowMs)
{
    const qint64 keep = qMax(kWindowMs, m_learned.value(key).second);
    QQueue<qint64> &q = m_sent[key];
    q.enqueue(nowMs);
    while (!q.isEmpty() && nowMs - q.head() >= keep)
        q.dequeue();
}

void RateLimiter::learn(const QString &key, int count, qint64 periodMs)
{
    if (count > 0 && periodMs > 0)
        m_learned[key] = qMakePair(count, periodMs);
}

void RateLimiter::blockUntil(const QString &key, qint64 untilMs)
{
    m_blockedUntil[key] = qMax(m_blockedUntil.value(key, 0), untilMs);
}
