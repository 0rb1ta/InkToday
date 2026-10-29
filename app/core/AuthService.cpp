#include "AuthService.h"

#include "ApiClient.h"
#include "Log.h"

#include <QJsonObject>

namespace {
const qint64 kRefreshWindowSecs = 12 * 3600;
}

AuthService::AuthService(ApiClient *client, SessionStore *store, std::function<QDateTime()> now, QObject *parent)
    : QObject(parent)
    , m_client(client)
    , m_store(store)
    , m_now(std::move(now))
    , m_session(store->load())
{
    if (m_session.isValid(m_now()))
        m_client->setToken(m_session.token);
}

bool AuthService::isLoggedIn() const
{
    return m_session.isValid(m_now());
}

void AuthService::login(const QString &login, const QString &password, const QString &code)
{
    if (m_loginPending)
        return; // a double tap must not log in twice
    m_loginPending = true;
    QJsonObject body{{QStringLiteral("login"), login}, {QStringLiteral("password"), password}};
    if (!code.isEmpty())
        body.insert(QStringLiteral("code"), code);
    if (!m_session.trustedCode.isEmpty())
        body.insert(QStringLiteral("trustedCode"), m_session.trustedCode);

    const bool sentCode = !code.isEmpty();
    m_client->post(QStringLiteral("/v1/account/login-by-password"), body,
                   [this, sentCode](const QJsonDocument &doc, const ApiError *e) {
        m_loginPending = false;
        if (e) {
            if (e->code == QLatin1String("CodeNotValid") && !sentCode) {
                // Without a code this means the server just e-mailed one.
                emit codeRequired(QStringLiteral("Email"), e->message);
                return;
            }
            emit loginFailed(e->message);
            return;
        }
        const QJsonObject o = doc.object();
        const QString token = o.value(QStringLiteral("token")).toString();
        if (token.isEmpty()) {
            // twoFactorType is a string while a challenge is pending, the number 0 otherwise.
            const QJsonValue type = o.value(QStringLiteral("twoFactorType"));
            if (type.isString() && !type.toString().isEmpty()) {
                emit codeRequired(type.toString(), {});
                return;
            }
            Log::write(QStringLiteral("login: no token and no challenge in response"));
            emit loginFailed(QStringLiteral("Сервер не выдал токен входа."));
            return;
        }
        m_session.token = token;
        m_session.expires = QDateTime::fromString(o.value(QStringLiteral("expires")).toString(), Qt::ISODate);
        const QString trusted = o.value(QStringLiteral("trustedCode")).toString();
        if (!trusted.isEmpty())
            m_session.trustedCode = trusted;
        m_client->setToken(token);
        m_store->save(m_session);
        fetchUser();
    });
}

void AuthService::fetchUser()
{
    m_client->get(QStringLiteral("/v1/account/current-user"), [this](const QJsonDocument &doc, const ApiError *e) {
        if (!e) {
            const QJsonObject o = doc.object();
            const qint64 previous = m_session.userId;
            m_session.userId = qint64(o.value(QStringLiteral("id")).toDouble());
            // Also from 0 (unknown after a failed profile): clearing an empty library costs nothing,
            // keeping another account's books would.
            if (previous != m_session.userId)
                emit userChanged(m_session.userId);
            m_session.userName = o.value(QStringLiteral("userName")).toString();
            m_store->save(m_session);
        } else if (m_session.userId) {
            // Unknown who logged in: it may be another account, so the previous one's data goes.
            m_session.userId = 0;
            m_store->save(m_session);
            emit userChanged(0);
        }
        emit loggedIn();
    });
}

void AuthService::refreshIfNeeded(std::function<void(bool ok)> done)
{
    if (m_session.token.isEmpty()) {
        done(false);
        return;
    }
    if (m_now().secsTo(m_session.expires) > kRefreshWindowSecs) {
        done(true);
        return;
    }
    refresh(done);
}

void AuthService::recover()
{
    if (m_recovering || m_refreshing || m_session.token.isEmpty())
        return; // also the refresh request's own 401: that refresh decides
    m_recovering = true;
    refresh([this](bool ok) {
        m_recovering = false;
        if (!ok && !m_session.token.isEmpty())
            dropToken(); // a network error during the retry: the refused token is still no good
    });
}

void AuthService::refresh(std::function<void(bool ok)> done)
{
    m_refreshing = true;
    m_client->setToken(m_session.token);
    m_client->post(QStringLiteral("/v1/account/refresh-token"), {}, [this, done](const QJsonDocument &doc, const ApiError *e) {
        m_refreshing = false;
        const QString token = e ? QString() : doc.object().value(QStringLiteral("token")).toString();
        if (token.isEmpty()) {
            if (e && e->httpStatus == 401) {
                dropToken();
                done(false);
                return;
            }
            const bool stillValid = m_session.isValid(m_now());
            if (!stillValid)
                m_client->setToken(QString()); // back to guest so guest endpoints keep working
            done(stillValid);
            return;
        }
        m_session.token = token;
        m_session.expires = QDateTime::fromString(doc.object().value(QStringLiteral("expires")).toString(), Qt::ISODate);
        m_client->setToken(token);
        m_store->save(m_session);
        done(true);
    });
}

void AuthService::dropToken()
{
    if (m_session.token.isEmpty())
        return; // already dropped
    Log::write(QStringLiteral("session lost: the site refused the token"));
    m_session.token.clear();
    m_session.expires = QDateTime();
    m_store->save(m_session);
    m_client->setToken(QString());
    emit sessionLost();
}

void AuthService::logout()
{
    m_session = Session();
    m_store->clear();
    m_client->setToken(QString());
    emit loggedOut();
}
