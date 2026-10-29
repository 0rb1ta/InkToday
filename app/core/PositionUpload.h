#pragma once

#include "Models.h"
#include "ReadingPosition.h"

#include <QHash>
#include <QObject>
#include <QPair>

#include <functional>

class ApiClient;


struct UploadDecision {
    bool send = false;
    qint64 chapterId = 0;
    double chapterPercent = 0; // 0...100
    double workPercent = 0;    // 0...100, by the site's chapter lengths
};

// The newer place wins: send when the reader saved its place more than kClockSlackSecs after the
// site's lastReadTime and the place differs (another chapter, or the same one by at least 1 %). Without
// either time, send only when the reader is ahead. Reading on the phone after the e-reader must
// never be overwritten by the e-reader's older place.
// Where «Открыть» put the reader (an estimated page): until the reader moves past it, its place
// there is not sent — it would move the site back a little, or into the chapter before.
struct OpenedAt {
    qint64 chapterId = 0;
    double percent = 0;
};
UploadDecision decideUpload(const WorkDetails &details, qint64 readerChapterId, double readerChapterPercent,
                            const QDateTime &readerSavedAt = QDateTime(), const OpenedAt *opened = nullptr);

// GET /v1/reader/start (a reading session), then POST /v1/reader/update-progress with its id: the
// place shows in the book's details and the site's progress list. Remembers what was sent so the
// same place is not sent twice, across restarts.
class PositionUploader : public QObject {
    Q_OBJECT
public:
    PositionUploader(ApiClient *client, const QString &stateFile, QObject *parent = nullptr);
    bool alreadySent(qint64 workId, const UploadDecision &d) const;
    void upload(qint64 workId, const UploadDecision &d, std::function<void(bool ok)> done);
    void rememberOpenedAt(qint64 workId, const OpenedAt &at) { m_opened.insert(workId, at); }
    const OpenedAt *openedAt(qint64 workId) const
    {
        auto it = m_opened.constFind(workId);
        return it == m_opened.constEnd() ? nullptr : &*it;
    }

private:
    void send(qint64 workId, const UploadDecision &d, const QString &session, std::function<void(bool)> done);
    void save() const;

    ApiClient *m_client;
    QString m_stateFile;
    QHash<qint64, QPair<qint64, double>> m_sent; // workId -> chapterId, chapter percent
    QHash<qint64, OpenedAt> m_opened; // this session only
};
