#include "ApiClient.h"

#include "Log.h"

#include <QTimer>

namespace {

const QString kApiBase = QStringLiteral("https://api.author.today");
const QString kApiHost = QStringLiteral("api.author.today");
const QByteArray kUserAgent = QByteArrayLiteral("InkToday/" APP_VERSION " (PocketBook; +https://github.com/0rb1ta/InkToday)");
const int kMaxRetries = 3;
const qint64 kMinRetryMs = 1000;
// A 429 asking to wait longer than this (an hourly limit) is reported, not retried silently.
const qint64 kMaxRetryWaitMs = 60000;

void setHeader(HttpHeaders &headers, const QByteArray &name, const QByteArray &value)
{
    for (auto &h : headers) {
        if (h.first.compare(name, Qt::CaseInsensitive) == 0) {
            h.second = value;
            return;
        }
    }
    headers.append({name, value});
}

} // namespace

namespace {

// The log is shown in the app and may end up in a bug report: no query strings (search text),
// and only host and path of other hosts (download links may be signed).
QString urlForLog(const QUrl &url)
{
    return url.host() + url.path();
}

} // namespace

ApiClient::ApiClient(HttpTransport *transport, std::function<qint64()> nowMs, QObject *parent)
    : QObject(parent)
    , m_transport(transport)
    , m_nowMs(std::move(nowMs))
{
}

void ApiClient::setToken(const QString &token)
{
    m_token = token;
}

QString ApiClient::token() const
{
    return m_token.isEmpty() ? QStringLiteral("guest") : m_token;
}

static void toJson(const HttpResponse &r, const ApiError *error, const ApiClient::JsonCb &done)
{
    if (error) {
        done(QJsonDocument(), error);
        return;
    }
    done(QJsonDocument::fromJson(r.body), nullptr);
}

void ApiClient::get(const QString &pathAndQuery, JsonCb done)
{
    HttpRequest req{"GET", QUrl(kApiBase + pathAndQuery), {{"Accept", "application/json"}}, {}};
    dispatch(req, [done](const HttpResponse &r, const ApiError *e) { toJson(r, e, done); }, kMaxRetries);
}

void ApiClient::post(const QString &path, const QJsonObject &body, JsonCb done)
{
    HttpRequest req{"POST", QUrl(kApiBase + path),
                    {{"Accept", "application/json"}, {"Content-Type", "application/json"}},
                    QJsonDocument(body).toJson(QJsonDocument::Compact)};
    dispatch(req, [done](const HttpResponse &r, const ApiError *e) { toJson(r, e, done); }, kMaxRetries);
}

void ApiClient::getRaw(const QUrl &absoluteOrApiPath, RawCb done, const HttpHeaders &extraHeaders)
{
    const QUrl url = absoluteOrApiPath.isRelative() ? QUrl(kApiBase + absoluteOrApiPath.toString())
                                                    : absoluteOrApiPath;
    HttpRequest req{"GET", url, {}, {}};
    for (const auto &h : extraHeaders)
        setHeader(req.headers, h.first, h.second);
    dispatch(req, std::move(done), kMaxRetries);
}

void ApiClient::dispatch(HttpRequest request, RawCb done, int retriesLeft)
{
    const bool toApi = request.url.host() == kApiHost;
    HttpHeaders headers = request.headers;
    bool hasUa = false;
    for (const auto &h : headers)
        hasUa |= h.first.compare("User-Agent", Qt::CaseInsensitive) == 0;
    if (!hasUa)
        headers.append({"User-Agent", kUserAgent});
    if (toApi)
        setHeader(headers, "Authorization", "Bearer " + token().toUtf8());
    request.headers = headers;

    const QString key = toApi ? RateLimiter::endpointKey(request.url.path())
                              : request.url.host() + request.url.path();
    auto send = [this, request, done, retriesLeft, key]() {
        m_limiter.record(key, m_nowMs());
        m_transport->send(request, [this, request, done, retriesLeft, key](const HttpResponse &r) {
            if (!r.networkError.isEmpty()) {
                Log::write(QStringLiteral("%1 %2 -> network error: %3%4")
                               .arg(QString::fromLatin1(request.method), urlForLog(request.url), r.networkError,
                                    r.timedOut ? QStringLiteral(" (timeout)") : QString()));
                const ApiError e = ApiError::networkError(r.networkError, r.timedOut);
                done(r, &e);
                return;
            }
            if (r.status >= 200 && r.status < 300) {
                done(r, nullptr);
                return;
            }
            const ApiError e = ApiError::fromHttp(r.status, r.body);
            Log::write(QStringLiteral("%1 %2 -> %3 %4")
                           .arg(QString::fromLatin1(request.method), urlForLog(request.url))
                           .arg(r.status)
                           .arg(e.code));
            // The site revoked or expired the token (password change, long pause): one place handles it.
            if (r.status == 401 && !m_token.isEmpty()
                && (e.code == QLatin1String("InvalidToken") || e.code == QLatin1String("ExpiredToken")))
                emit unauthorized(e.code);
            if (e.code == QLatin1String("VersionIsUnsupported"))
                Log::write(QStringLiteral("VersionIsUnsupported, full answer: %1").arg(QString::fromUtf8(r.body.left(2000))));
            if (r.status == 429)
                m_limiter.learn(key, e.rateLimit, e.periodMs);
            // Never retry sooner than a second: "per Second" limits come with retryAfter 0 or none.
            const qint64 waitMs = qMax<qint64>(kMinRetryMs, qMax(e.retryAfterSec * 1000LL, e.periodMs));
            if (r.status == 429)
                m_limiter.blockUntil(key, m_nowMs() + waitMs); // also for a long wait: no more 429s meanwhile
            if (r.status == 429 && retriesLeft > 0 && waitMs <= kMaxRetryWaitMs) {
                dispatch(request, done, retriesLeft - 1);
                return;
            }
            done(r, &e);
        });
    };

    const qint64 delay = m_limiter.delayMs(key, m_nowMs());
    if (delay > kMaxRetryWaitMs) {
        // An hourly limit is reached: say so now instead of parking the request for up to an hour.
        ApiError e = ApiError::local(QStringLiteral("TooManyRequests"),
                                     QStringLiteral("Слишком много запросов, попробуйте позже."));
        e.httpStatus = 429;
        done(HttpResponse{429, {}, {}, {}}, &e);
        return;
    }
    if (delay <= 0)
        send(); // immediate, so synchronous transports stay synchronous (the callback runs before dispatch() returns)
    else
        QTimer::singleShot(int(delay), this, send);
}
