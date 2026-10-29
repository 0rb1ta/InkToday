#include "Downloader.h"

#include "ApiClient.h"
#include "DownloadIndex.h"
#include "FileNaming.h"
#include "Log.h"
#include "Models.h"

#include <QDateTime>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QJsonDocument>
#include <QJsonObject>

#include <unistd.h>

namespace {

bool isZip(const QByteArray &body)
{
    return body.startsWith("PK"); // EPUB is a zip archive
}

// A complete zip ends with an end-of-central-directory record (22 bytes + up to 64 KB comment).
// A transfer cut short keeps the leading "PK" but loses it.
bool zipIsComplete(const QByteArray &body)
{
    const int tail = qMin(body.size(), 22 + 65535);
    return body.right(tail).contains(QByteArray("PK\x05\x06", 4));
}

bool looksLikeBook(const QByteArray &body, const QString &format)
{
    if (isZip(body))
        return true;
    return format == QLatin1String("Fb2") && body.trimmed().startsWith("<?xml");
}

} // namespace

QString chooseBookPath(const QString &booksDir, const QString &fileName, qint64 workId, const QString &currentPathOfThisWork,
                       qint64 currentSize)
{
    const QFileInfo fi(fileName);
    const QFileInfo current(currentPathOfThisWork);
    // Ours still: the file this book was saved to, same folder and format, not replaced by
    // another file since (its size is what we wrote). Updating it in place keeps the reader's
    // place even when the author renamed the book.
    const bool ours = !currentPathOfThisWork.isEmpty() && current.isFile()
        && QFileInfo(current.absolutePath()) == QFileInfo(booksDir)
        && current.suffix().compare(fi.suffix(), Qt::CaseInsensitive) == 0
        && (currentSize < 0 || current.size() == currentSize);
    if (ours)
        return currentPathOfThisWork;
    const QString plain = booksDir + QLatin1Char('/') + fileName;
    bool taken = false;
    for (const QString &existing : QDir(booksDir).entryList(QDir::Files))
        taken |= existing.compare(fileName, Qt::CaseInsensitive) == 0;
    if (!taken)
        return plain;
    return booksDir + QLatin1Char('/') + fi.completeBaseName() + QStringLiteral(" (%1).").arg(workId) + fi.suffix();
}

void recoverInterruptedReplace(const QString &target)
{
    const QString old = target + QStringLiteral(".old");
    if (!QFile::exists(target) && QFileInfo(old).isFile()) {
        Log::write(QStringLiteral("restoring %1 after an interrupted replace").arg(target));
        QFile::rename(old, target);
    }
    if (QFileInfo(old).isFile())
        QFile::remove(old);
    QFile::remove(target + QStringLiteral(".part"));
}

// Failures are logged with the OS reason: they are otherwise invisible on the device.
bool replaceFileSafely(const QString &target, const QByteArray &data)
{
    recoverInterruptedReplace(target);
    const QString part = target + QStringLiteral(".part");
    const QString old = target + QStringLiteral(".old");
    QFile f(part);
    if (!f.open(QIODevice::WriteOnly)) {
        Log::write(QStringLiteral("write failed: open %1: %2").arg(part, f.errorString()));
        return false;
    }
    const bool written = f.write(data) == data.size() && f.flush() && ::fsync(f.handle()) == 0;
    if (!written) {
        Log::write(QStringLiteral("write failed: write %1: %2").arg(part, f.errorString()));
        f.close();
        QFile::remove(part);
        return false;
    }
    f.close();
    const bool hadOld = QFile::exists(target);
    if (hadOld) {
        if (QFileInfo(old).isFile())
            QFile::remove(old); // left over from an interrupted swap
        if (!QFile::rename(target, old)) {
            Log::write(QStringLiteral("write failed: cannot move old %1 aside").arg(target));
            QFile::remove(part);
            return false;
        }
    }
    if (!QFile::rename(part, target)) {
        Log::write(QStringLiteral("write failed: rename %1 -> %2").arg(part, target));
        QFile::remove(part);
        if (hadOld)
            QFile::rename(old, target);
        return false;
    }
    if (hadOld)
        QFile::remove(old);
    return true;
}

Downloader::Downloader(ApiClient *client, DownloadIndex *index, QObject *parent)
    : QObject(parent)
    , m_client(client)
    , m_index(index)
{
}

void Downloader::download(const WorkDetails &work, const QString &booksDir, const QString &format,
                          std::function<void(const QString &, const ApiError *)> done)
{
    if (!work.allowDownloads) {
        // The author's choice is final here: no request, no file.
        const ApiError e = ApiError::local(QStringLiteral("DownloadDisabled"),
                                           work.downloadErrorMessage.isEmpty()
                                               ? QStringLiteral("Автор закрыл скачивание этой книги.")
                                               : work.downloadErrorMessage);
        done(QString(), &e);
        return;
    }
    const QString ext = format == QLatin1String("Fb2") ? QStringLiteral("fb2") : QStringLiteral("epub");
    const DownloadEntry before = m_index->contains(work.meta.id) ? m_index->get(work.meta.id) : DownloadEntry();
    const QString target = chooseBookPath(booksDir, bookFileName(work.meta.authors, work.meta.title, ext), work.meta.id,
                                          before.path, before.path.isEmpty() ? -1 : before.size);
    const QString path = QStringLiteral("/v1/work/%1/download?type=%2&trial=false").arg(work.meta.id).arg(format);
    m_client->getRaw(QUrl(path), [=](const HttpResponse &r, const ApiError *e) {
        if (e) {
            done(QString(), e);
            return;
        }
        handleBody(work, target, format, r, true, done);
    });
}

void Downloader::handleBody(const WorkDetails &work, const QString &target, const QString &format,
                            const HttpResponse &r, bool mayFollowLink,
                            std::function<void(const QString &, const ApiError *)> done)
{
    if (looksLikeBook(r.body, format)) {
        if (isZip(r.body) && !zipIsComplete(r.body)) {
            Log::write(QStringLiteral("download of work %1 is incomplete (%2 bytes, no zip end record)")
                           .arg(work.meta.id).arg(r.body.size()));
            const ApiError e = ApiError::local(QStringLiteral("IncompleteDownload"),
                                               QStringLiteral("Книга скачалась не полностью, попробуйте ещё раз."));
            done(QString(), &e);
            return;
        }
        if (!replaceFileSafely(target, r.body)) {
            const ApiError e = ApiError::local(QStringLiteral("WriteFailed"),
                                               QStringLiteral("Не удалось сохранить книгу на читалку."));
            done(QString(), &e);
            return;
        }
        if (m_index->contains(work.meta.id)) {
            // Saved before in the other format: the old copy would stay as a second book.
            const DownloadEntry before = m_index->get(work.meta.id);
            const QFileInfo was(before.path);
            if (before.path != target && was.isFile() && was.size() == before.size && QFile::remove(before.path))
                Log::write(QStringLiteral("removed the earlier copy %1").arg(before.path));
        }
        m_index->put({work.meta.id, target, format, work.meta.lastModificationTime, r.body.size(),
                      QDateTime::currentDateTimeUtc(), work.chapters.size()});
        Log::write(QStringLiteral("downloaded work %1 -> %2 (%3 bytes)").arg(work.meta.id).arg(target).arg(r.body.size()));
        done(target, nullptr);
        return;
    }

    const QJsonObject o = QJsonDocument::fromJson(r.body).object();
    if (o.contains(QStringLiteral("code"))) {
        const ApiError e = ApiError::fromHttp(r.status, r.body);
        done(QString(), &e);
        return;
    }
    if (mayFollowLink) {
        for (const char *key : {"url", "downloadUrl", "link", "fileUrl"}) {
            const QString link = o.value(QLatin1String(key)).toString();
            if (link.isEmpty())
                continue;
            Log::write(QStringLiteral("download of work %1 answered with a link").arg(work.meta.id));
            m_client->getRaw(QUrl(link), [=](const HttpResponse &r2, const ApiError *e2) {
                if (e2) {
                    done(QString(), e2);
                    return;
                }
                handleBody(work, target, format, r2, false, done);
            });
            return;
        }
    }
    Log::write(QStringLiteral("unexpected download response for work %1 (%2): %3")
                   .arg(work.meta.id)
                   .arg(QString::fromLatin1(r.contentType))
                   .arg(QString::fromUtf8(r.body.left(300))));
    const ApiError e = ApiError::local(QStringLiteral("UnexpectedDownloadResponse"),
                                       QStringLiteral("Сервер вернул не файл книги."));
    done(QString(), &e);
}
