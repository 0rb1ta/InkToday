#pragma once

#include <QDateTime>
#include <QString>

// A book's place in the reader's own library (system/explorer-3/explorer-3.db).
struct ReaderPosition {
    bool found = false;   // false: never opened in the reader, not rescanned yet, or no database
    QString position;     // books_settings.position, e.g. "pbr:/webkit?##epubcfi(/6/10!/4/2/5:9)"
    int page = 0;         // cpage
    int pages = 0;        // npage
    QDateTime savedAt;    // position_ts (UTC)
    bool completed = false;
};

// Reads the firmware's library without touching it: the database and its WAL are copied to
// `copyDir` (only when they changed since the last copy) and the copy is queried.
class ReaderLibrary {
public:
    ReaderLibrary(const QString &dbPath, const QString &copyDir);
    ReaderPosition positionOf(const QString &bookPath);
    // Sizes and times of the reader's files: changes whenever the reader saves something.
    QString currentStamp() const;

private:
    bool refreshCopy();

    QString m_dbPath;
    QString m_copyDir;
    QString m_stamp; // sizes and times of the source files at the last copy
};
