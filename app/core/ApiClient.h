#pragma once

#include "ApiError.h"
#include "HttpTransport.h"
#include "RateLimiter.h"

#include <QJsonDocument>
#include <QJsonObject>
#include <QObject>

// author.today API client: auth header, our User-Agent, per-endpoint rate limiting,
// up to 3 retries after 429 (learning the endpoint's real limit from the answer).
// Callbacks get `error == nullptr` on success.
class ApiClient : public QObject {
    Q_OBJECT
public:
    using JsonCb = std::function<void(const QJsonDocument &, const ApiError *)>;
    using RawCb = std::function<void(const HttpResponse &, const ApiError *)>;

    ApiClient(HttpTransport *transport, std::function<qint64()> nowMs, QObject *parent = nullptr);

    void setToken(const QString &token); // empty -> guest
    QString token() const;

    void get(const QString &pathAndQuery, JsonCb done);
    void post(const QString &path, const QJsonObject &body, JsonCb done);
    // Relative paths go to the API host; absolute URLs are fetched as-is, without the auth header.
    void getRaw(const QUrl &absoluteOrApiPath, RawCb done, const HttpHeaders &extraHeaders = {});

signals:
    // 401 InvalidToken / ExpiredToken for a request that carried a token.
    void unauthorized(const QString &code);

private:
    void dispatch(HttpRequest request, RawCb done, int retriesLeft);

    HttpTransport *m_transport;
    std::function<qint64()> m_nowMs;
    RateLimiter m_limiter;
    QString m_token;
};
