#include "PositionUpload.h"

#include "ApiClient.h"
#include "FileIO.h"
#include "Log.h"

#include <QFile>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>

UploadDecision decideUpload(const WorkDetails &details, qint64 readerChapterId, double readerChapterPercent,
                            const QDateTime &readerSavedAt, const OpenedAt *opened)
{
    UploadDecision d;
    int readerOrder = -1, siteOrder = -1;
    qint64 before = 0, total = 0, current = 0;
    for (const Chapter &c : details.chapters) {
        total += c.textLength;
        if (c.id == readerChapterId) {
            readerOrder = c.sortOrder;
            current = c.textLength;
        }
        if (c.id == details.lastChapterId)
            siteOrder = c.sortOrder;
    }
    if (readerOrder < 0 || total <= 0)
        return d; // without chapter lengths the book percent would go out as 0
    for (const Chapter &c : details.chapters)
        if (c.sortOrder < readerOrder)
            before += c.textLength;
    if (readerSavedAt.isValid() && details.lastReadTime.isValid() && siteOrder >= 0) {
        const bool samePlace = readerOrder == siteOrder && qAbs(readerChapterPercent - details.lastChapterProgress) < kSamePlacePercent;
        d.send = !samePlace && readerSavedAt > details.lastReadTime.addSecs(kClockSlackSecs);
    } else {
        d.send = siteOrder < 0 || readerOrder > siteOrder
            || (readerOrder == siteOrder && readerChapterPercent >= details.lastChapterProgress + kSamePlacePercent);
    }
    if (opened && d.send) {
        int openedOrder = -1, previous = -1;
        for (const Chapter &c : details.chapters)
            if (c.id == opened->chapterId)
                openedOrder = c.sortOrder;
        for (const Chapter &c : details.chapters)
            if (openedOrder >= 0 && c.sortOrder < openedOrder)
                previous = qMax(previous, c.sortOrder);
        const bool atOpened = readerOrder == openedOrder && readerChapterPercent < opened->percent + kSamePlacePercent;
        if (atOpened || (previous >= 0 && readerOrder == previous))
            d.send = false;
    }
    d.chapterId = readerChapterId;
    d.chapterPercent = readerChapterPercent;
    d.workPercent = 100.0 * (before + current * readerChapterPercent / 100.0) / total;
    return d;
}

PositionUploader::PositionUploader(ApiClient *client, const QString &stateFile, QObject *parent)
    : QObject(parent)
    , m_client(client)
    , m_stateFile(stateFile)
{
    QFile f(m_stateFile);
    if (!f.open(QIODevice::ReadOnly))
        return;
    for (const QJsonValue &v : QJsonDocument::fromJson(f.readAll()).array()) {
        const QJsonObject o = v.toObject();
        m_sent.insert(qint64(o.value(QStringLiteral("workId")).toDouble()),
                      qMakePair(qint64(o.value(QStringLiteral("chapterId")).toDouble()), o.value(QStringLiteral("percent")).toDouble()));
    }
}

bool PositionUploader::alreadySent(qint64 workId, const UploadDecision &d) const
{
    const auto it = m_sent.constFind(workId);
    return it != m_sent.constEnd() && it->first == d.chapterId && qAbs(it->second - d.chapterPercent) < 0.01;
}

void PositionUploader::upload(qint64 workId, const UploadDecision &d, std::function<void(bool)> done)
{
    if (alreadySent(workId, d)) {
        done(true);
        return;
    }
    // A reading session first, as the site's own readers do: without its id the place reaches the
    // book's details but not the progress list the phone apps sync from.
    const QString start = QStringLiteral("/v1/reader/start/%1/%2").arg(workId).arg(d.chapterId);
    m_client->get(start, [this, workId, d, done](const QJsonDocument &doc, const ApiError *e) {
        const QString session = e ? QString() : doc.object().value(QStringLiteral("sessionId")).toString();
        if (e)
            Log::write(QStringLiteral("reading session for work %1 failed: %2").arg(workId).arg(e->code));
        send(workId, d, session, done);
    });
}

void PositionUploader::send(qint64 workId, const UploadDecision &d, const QString &session, std::function<void(bool)> done)
{
    const QJsonObject body{{QStringLiteral("workId"), double(workId)},
                           {QStringLiteral("chapterId"), double(d.chapterId)},
                           {QStringLiteral("chapterProgress"), d.chapterPercent},
                           {QStringLiteral("workProgress"), d.workPercent},
                           {QStringLiteral("sessionId"), session}};
    m_client->post(QStringLiteral("/v1/reader/update-progress"), body, [this, workId, d, done](const QJsonDocument &, const ApiError *e) {
        if (e) {
            Log::write(QStringLiteral("position upload for work %1 failed: %2").arg(workId).arg(e->code));
            done(false);
            return;
        }
        Log::write(QStringLiteral("position upload: work %1 -> chapter %2, %3 %").arg(workId).arg(d.chapterId).arg(d.chapterPercent));
        m_sent.insert(workId, qMakePair(d.chapterId, d.chapterPercent));
        save();
        done(true);
    });
}

void PositionUploader::save() const
{
    QJsonArray arr;
    for (auto it = m_sent.constBegin(); it != m_sent.constEnd(); ++it)
        arr.append(QJsonObject{{QStringLiteral("workId"), double(it.key())},
                               {QStringLiteral("chapterId"), double(it->first)},
                               {QStringLiteral("percent"), it->second}});
    writeFileIfChanged(m_stateFile, QJsonDocument(arr).toJson(QJsonDocument::Compact));
}
