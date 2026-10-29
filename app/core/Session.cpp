#include "Session.h"

#include <QFile>
#include <QJsonDocument>
#include <QJsonObject>
#include "FileIO.h"

SessionStore::SessionStore(const QString &filePath)
    : m_path(filePath)
{
}

Session SessionStore::load() const
{
    QFile f(m_path);
    if (!f.open(QIODevice::ReadOnly))
        return {};
    const QJsonObject o = QJsonDocument::fromJson(f.readAll()).object();
    Session s;
    s.token = o.value(QStringLiteral("token")).toString();
    s.expires = QDateTime::fromString(o.value(QStringLiteral("expires")).toString(), Qt::ISODate);
    s.trustedCode = o.value(QStringLiteral("trustedCode")).toString();
    s.userId = qint64(o.value(QStringLiteral("userId")).toDouble());
    s.userName = o.value(QStringLiteral("userName")).toString();
    return s;
}

void SessionStore::save(const Session &s)
{
    const QJsonObject o{
        {QStringLiteral("token"), s.token},
        {QStringLiteral("expires"), s.expires.toUTC().toString(Qt::ISODate)},
        {QStringLiteral("trustedCode"), s.trustedCode},
        {QStringLiteral("userId"), double(s.userId)},
        {QStringLiteral("userName"), s.userName},
    };
    writeFileIfChanged(m_path, QJsonDocument(o).toJson());
    QFile::setPermissions(m_path, QFile::ReadOwner | QFile::WriteOwner);
}

void SessionStore::clear()
{
    QFile::remove(m_path);
}
