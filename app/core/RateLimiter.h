#pragma once

#include <QHash>
#include <QQueue>
#include <QString>

// author.today allows ~20 requests per minute per endpoint per IP, and some endpoints less
// (user-library: 1 per second). Tracks a sliding window per endpoint key and answers how long
// the next request must wait.
class RateLimiter {
public:
    explicit RateLimiter(int perMinute = 20);

    // "/v1/work/671044/details?x=1" -> "/v1/work/{id}/details"
    static QString endpointKey(const QString &pathAndQuery);

    qint64 delayMs(const QString &key, qint64 nowMs) const;
    void record(const QString &key, qint64 nowMs);
    void blockUntil(const QString &key, qint64 untilMs);
    // The server told us this endpoint's real limit (e.g. user-library: 1 per Second).
    void learn(const QString &key, int count, qint64 periodMs);

private:
    int m_perMinute;
    QHash<QString, QQueue<qint64>> m_sent;
    QHash<QString, qint64> m_blockedUntil;
    QHash<QString, QPair<int, qint64>> m_learned; // key -> (count, period)
};
