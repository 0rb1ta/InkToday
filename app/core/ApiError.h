#pragma once

#include <QByteArray>
#include <QString>

// A failed API call. `message` is user-facing (the server writes it in Russian for end users).
struct ApiError {
    int httpStatus = 0;
    QString code;
    QString message;
    int retryAfterSec = -1;
    int rateLimit = 0;   // from a 429: allowed requests per period
    qint64 periodMs = 0; // from a 429: "Second" / "Minute" / "Hour"
    bool network = false;
    QString detail; // for the log: the transport's own (English) text

    static ApiError fromHttp(int status, const QByteArray &body);
    // `message` is Russian for the user; the Qt text goes to `detail`.
    static ApiError networkError(const QString &detail, bool timedOut = false);
    static ApiError local(const QString &code, const QString &msg);
};
