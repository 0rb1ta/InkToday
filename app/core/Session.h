#pragma once

#include <QDateTime>
#include <QString>

// Signed-in state. `trustedCode` lets later logins skip the e-mail code challenge.
struct Session {
    QString token;
    QDateTime expires;
    QString trustedCode;
    qint64 userId = 0;
    QString userName;

    bool isValid(const QDateTime &now) const { return !token.isEmpty() && expires.isValid() && expires > now; }
};

class SessionStore {
public:
    explicit SessionStore(const QString &filePath);
    Session load() const;
    void save(const Session &session);
    void clear();

private:
    QString m_path;
};
