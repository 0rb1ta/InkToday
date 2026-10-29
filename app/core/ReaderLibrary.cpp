#include "ReaderLibrary.h"

#include "Log.h"

#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QSqlDatabase>
#include <QSqlError>
#include <QSqlQuery>
#include <QVariant>

namespace {

QString stampOf(const QString &path)
{
    const QFileInfo fi(path);
    return fi.exists() ? QStringLiteral("%1@%2").arg(fi.size()).arg(fi.lastModified().toMSecsSinceEpoch()) : QStringLiteral("-");
}

} // namespace

ReaderLibrary::ReaderLibrary(const QString &dbPath, const QString &copyDir)
    : m_dbPath(dbPath)
    , m_copyDir(copyDir)
{
}

QString ReaderLibrary::currentStamp() const
{
    return stampOf(m_dbPath) + QLatin1Char('|') + stampOf(m_dbPath + QStringLiteral("-wal"));
}

bool ReaderLibrary::refreshCopy()
{
    if (!QFileInfo(m_dbPath).isFile())
        return false;
    const QString wal = m_dbPath + QStringLiteral("-wal");
    const QString stamp = currentStamp();
    const QString copy = m_copyDir + QStringLiteral("/explorer-3.db");
    if (stamp == m_stamp && QFile::exists(copy))
        return true;
    QDir().mkpath(m_copyDir);
    for (const QString &suffix : {QString(), QStringLiteral("-wal"), QStringLiteral("-shm")})
        QFile::remove(copy + suffix);
    if (!QFile::copy(m_dbPath, copy)) {
        Log::write(QStringLiteral("reader library: cannot copy %1").arg(m_dbPath));
        return false;
    }
    if (QFile::exists(wal))
        QFile::copy(wal, copy + QStringLiteral("-wal")); // the latest positions live in the WAL
    m_stamp = stamp;
    return true;
}

ReaderPosition ReaderLibrary::positionOf(const QString &bookPath)
{
    ReaderPosition p;
    if (!refreshCopy())
        return p;
    const QFileInfo book(bookPath);
    const QString name = QStringLiteral("reader-library-%1").arg(quintptr(this));
    {
        // Our own copy: opened normally (SQLite may merge the WAL into it; the firmware's file is untouched).
        QSqlDatabase db = QSqlDatabase::addDatabase(QStringLiteral("QSQLITE"), name);
        db.setDatabaseName(m_copyDir + QStringLiteral("/explorer-3.db"));
        if (!db.open()) {
            Log::write(QStringLiteral("reader library: open failed: %1").arg(db.lastError().text()));
        } else {
            QSqlQuery q(db);
            // By folder path, not storage id: internal memory and the SD card both count; books in
            // the cloud have no folder name.
            q.prepare(QStringLiteral(
                "SELECT s.position, s.position_ts, s.cpage, s.npage, s.completed "
                "FROM files f JOIN folders fo ON fo.id = f.folder_id "
                "JOIN books_settings s ON s.bookid = f.book_id AND s.profileid = 1 "
                "WHERE fo.name = ? AND f.filename = ?"));
            q.addBindValue(book.absolutePath());
            q.addBindValue(book.fileName());
            if (!q.exec())
                Log::write(QStringLiteral("reader library: query failed: %1").arg(q.lastError().text()));
            else if (q.next()) {
                p.found = true;
                p.position = q.value(0).toString();
                p.savedAt = QDateTime::fromSecsSinceEpoch(q.value(1).toLongLong(), Qt::UTC);
                p.page = q.value(2).toInt();
                p.pages = q.value(3).toInt();
                p.completed = q.value(4).toInt() != 0;
            }
            db.close();
        }
    }
    QSqlDatabase::removeDatabase(name);
    return p;
}
