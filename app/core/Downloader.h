#pragma once

#include <QObject>

#include <functional>

class ApiClient;
class DownloadIndex;
struct ApiError;
struct HttpResponse;
struct WorkDetails;

// Downloads the author-provided book file (GET /v1/work/{id}/download) into the books folder.
// Refuses outright when the author disabled downloads; never leaves a partial book behind.
// Where a book goes. This book's earlier file (same folder and format, still `currentSize` bytes)
// is updated in place, even under an old title. Otherwise `booksDir/fileName`, unless another
// file already has that name (FAT32 ignores case); then "<name> (<workId>).<ext>", so two books
// never overwrite each other or a file of the owner.
QString chooseBookPath(const QString &booksDir, const QString &fileName, qint64 workId, const QString &currentPathOfThisWork,
                       qint64 currentSize = -1);

// Writes next to the target, syncs it to the card and swaps it in; the old file is kept until the
// new one is in place, and comes back if the swap fails.
bool replaceFileSafely(const QString &target, const QByteArray &data);

// After a crash in the middle of replaceFileSafely: puts the old book back if the new one never
// arrived, and removes the leftover .part / .old files. Called before every write.
void recoverInterruptedReplace(const QString &target);

class Downloader : public QObject {
    Q_OBJECT
public:
    Downloader(ApiClient *client, DownloadIndex *index, QObject *parent = nullptr);

    // format: "Epub" or "Fb2" (API values). done(path, nullptr) on success.
    void download(const WorkDetails &work, const QString &booksDir, const QString &format,
                  std::function<void(const QString &path, const ApiError *)> done);

private:
    void handleBody(const WorkDetails &work, const QString &target, const QString &format,
                    const HttpResponse &response, bool mayFollowLink,
                    std::function<void(const QString &path, const ApiError *)> done);

    ApiClient *m_client;
    DownloadIndex *m_index;
};
