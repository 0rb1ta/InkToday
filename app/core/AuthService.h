#pragma once

#include "Session.h"

#include <QObject>

#include <functional>

class ApiClient;

// Login by password with the e-mail code challenge, token refresh, logout to guest.
class AuthService : public QObject {
    Q_OBJECT
public:
    AuthService(ApiClient *client, SessionStore *store, std::function<QDateTime()> now, QObject *parent = nullptr);

    bool isLoggedIn() const;
    bool hasToken() const { return !m_session.token.isEmpty(); } // stored, maybe past its time
    Session session() const { return m_session; }

    void login(const QString &login, const QString &password, const QString &code = {});
    // Renews the token when fewer than 12 hours remain. ok == false: the user must log in again.
    void refreshIfNeeded(std::function<void(bool ok)> done);
    void logout();
    // A request was refused with 401 InvalidToken/ExpiredToken: renew once, or drop the token.
    void recover();

signals:
    void loggedIn();
    void codeRequired(const QString &twoFactorType, const QString &serverMessage);
    void loginFailed(const QString &message);
    void loggedOut();
    // A different account than the one last signed in on this reader: cached data must be dropped.
    void userChanged(qint64 userId);
    // The site no longer accepts the token (password changed, revoked): back to guest. The same
    // account, so its library stays; the e-mail trust code stays too.
    void sessionLost();

private:
    void fetchUser();
    void refresh(std::function<void(bool ok)> done);
    void dropToken();
    bool m_loginPending = false;
    bool m_recovering = false;
    bool m_refreshing = false;

    ApiClient *m_client;
    SessionStore *m_store;
    std::function<QDateTime()> m_now;
    Session m_session;
};
