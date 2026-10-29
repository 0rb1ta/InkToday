#pragma once

#include "BookStatus.h"

#include <QObject>
#include <QPointer>

#include <functional>

class AppContext;
class QWidget;
struct ApiError;
struct WorkDetails;

// What pressing a book's action button does, the same on every screen:
// read / download (+open) / update with a reading-position warning / buy or site-only QR.
class BookActions : public QObject {
    Q_OBJECT
public:
    explicit BookActions(AppContext *ctx, QObject *parent = nullptr);
    void run(qint64 workId, ActionState state, QWidget *parent);
    // One confirmation, then the books are downloaded one after another ("Обновить все").
    void updateAll(const QList<qint64> &workIds, QWidget *parent);

signals:
    void openCard(qint64 workId);
    void loginRequested();

private:
    // Setting: when the place last read on the site is newer than the reader's, asks where to open.
    // Asks the site first, since the cached details may be old or lack chapters.
    void openAtSiteChapter(qint64 workId, const QString &path, QWidget *parent);
    void askAndOpen(qint64 workId, const QString &path, QPointer<QWidget> guard);
    // `then(ok)` runs once the download ended either way (for "Обновить все").
    void download(qint64 workId, QWidget *parent, bool open = true, std::function<void(bool ok)> then = {});
    void withFreshDetails(qint64 workId, std::function<void(const WorkDetails &)> then,
                          std::function<void(const ApiError &)> failed);

    AppContext *m_ctx;
    bool m_busy = false;
    qint64 m_busyId = 0;       // the book being downloaded
    bool m_updatingAll = false; // «Обновить все» is running
    bool m_opening = false;
};
