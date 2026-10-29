#include "BookActions.h"

#include "ApiClient.h"
#include "AppContext.h"
#include "AuthService.h"
#include "Device.h"
#include "DownloadIndex.h"
#include "Downloader.h"
#include "EpubToc.h"
#include "Log.h"
#include "PositionUpload.h"
#include "ReaderLibrary.h"
#include "ReadingPosition.h"
#include "Settings.h"
#include "StatusCache.h"
#include "Widgets.h"

#include <QPointer>

#include <memory>

namespace {

QString workUrl(qint64 id)
{
    return QStringLiteral("https://author.today/work/%1").arg(id);
}

} // namespace

BookActions::BookActions(AppContext *ctx, QObject *parent)
    : QObject(parent)
    , m_ctx(ctx)
{
}

void BookActions::openAtSiteChapter(qint64 workId, const QString &path, QWidget *parent)
{
    if (m_opening)
        return;
    m_opening = true;
    Ui::toast(parent, QStringLiteral("Открываю книгу…"));
    QPointer<QWidget> guard(parent);
    // The firmware turns Wi-Fi off on its own: connect first, as sync does.
    m_ctx->networkGate()([this, workId, path, guard](bool online) {
        if (!online) {
            m_opening = false;
            Log::write(QStringLiteral("open work %1 as usual: no network").arg(workId));
            device::openBook(path);
            return;
        }
        askAndOpen(workId, path, guard);
    });
}

void BookActions::askAndOpen(qint64 workId, const QString &path, QPointer<QWidget> guard)
{
    withFreshDetails(workId, [this, workId, path, guard](const WorkDetails &details) {
        m_opening = false;
        const ReaderPosition reader = m_ctx->readerLibrary ? m_ctx->readerLibrary->positionOf(path) : ReaderPosition();
        const QList<int> lengths = readEpubTextLengths(path);
        const SiteOffer o = siteOffer(details, mapChapters(readEpubToc(path), details.chapters, lengths.size()), reader,
                                      lengths, [path](int spine) { return readEpubSpineFile(path, spine); });
        if (o.needsLayout && guard)
            Ui::toast(guard, QStringLiteral("На сайте вы дальше: «%1». Откройте её через оглавление книги").arg(o.siteChapter));
        if (o.position.isEmpty() || !guard) {
            Log::write(QStringLiteral("open work %1 as usual: %2").arg(workId).arg(guard ? o.whyNot : QStringLiteral("screen closed")));
            device::openBook(path);
            return;
        }
        const QDateTime now = QDateTime::currentDateTime();
        const int percent = qRound(o.sitePercent);
        QString site = QStringLiteral("Сайт: «%1», %2 %").arg(o.siteChapter).arg(percent);
        QString here = !o.readerChapter.isEmpty() ? QStringLiteral("Читалка: «%1»").arg(o.readerChapter)
                       : reader.found && reader.pages > 0 ? QStringLiteral("Читалка: стр. %1 из %2").arg(reader.page).arg(reader.pages)
                                                          : QStringLiteral("Читалка: книга ещё не открывалась");
        if (o.byTime) {
            site += QStringLiteral(" — ") + whenText(o.siteTime, now);
            here += QStringLiteral(" — ") + whenText(o.readerTime, now);
        }
        const int answer = Ui::ask(guard, o.byTime ? QStringLiteral("На сайте вы читали позже") : QStringLiteral("На сайте вы дальше"),
                                   site + QLatin1Char('\n') + here, QStringLiteral("Открыть «%1»").arg(o.siteChapter),
                                   QStringLiteral("Где в читалке"));
        if (answer == 0) {
            Log::write(QStringLiteral("open work %1 at %2").arg(workId).arg(o.position));
            device::openBookAt(path, o.position);
            if (m_ctx->uploader) // an estimated page: not to be sent back before the reader moves on
                m_ctx->uploader->rememberOpenedAt(workId, OpenedAt{details.lastChapterId, details.lastChapterProgress});
        } else if (answer == 1) {
            device::openBook(path);
        }
    }, [this, workId, path](const ApiError &e) {
        m_opening = false;
        Log::write(QStringLiteral("open work %1 as usual: details failed: %2").arg(workId).arg(e.code));
        device::openBook(path);
    });
}

void BookActions::run(qint64 workId, ActionState state, QWidget *parent)
{
    switch (state) {
    case ActionState::Read: {
        const DownloadEntry e = m_ctx->index->get(workId);
        if (e.path.isEmpty())
            return;
        if (m_ctx->settings->openAtSiteChapter && m_ctx->auth->isLoggedIn())
            openAtSiteChapter(workId, e.path, parent);
        else
            device::openBook(e.path);
        return;
    }
    case ActionState::Download:
        if (!m_ctx->auth->isLoggedIn()) {
            if (Ui::confirm(parent, QStringLiteral("Чтобы скачать книгу, войдите в аккаунт author.today."), QStringLiteral("Войти")))
                emit loginRequested();
            return;
        }
        download(workId, parent);
        return;
    case ActionState::Update:
        // Replacing the file makes the reader treat it as a new book: the reading position starts over.
        if (Ui::confirm(parent, QStringLiteral("Место чтения в этой книге начнётся сначала. Обновить?"), QStringLiteral("Обновить")))
            download(workId, parent);
        return;
    case ActionState::Buy: {
        const WorkDetails d = m_ctx->status->get(workId);
        const QString subtitle = d.meta.price > 0 ? QStringLiteral("«%1» · %2 ₽").arg(d.meta.title).arg(d.meta.price)
                                                  : QStringLiteral("«%1»").arg(d.meta.title);
        if (Ui::showQr(parent, QStringLiteral("Купить на сайте"), subtitle, workUrl(workId),
                       QStringLiteral("1. Наведите камеру телефона на код.\n2. Оплатите книгу на сайте.\n"
                                      "3. Нажмите «Я оплатил» — приложение обновит статус книги."),
                       QStringLiteral("Я оплатил"))) {
            m_ctx->status->invalidate(workId);
            m_ctx->status->request({workId});
            emit m_ctx->syncRequested();
        }
        return;
    }
    case ActionState::Subscribers: {
        const WorkDetails d = m_ctx->status->get(workId);
        const QString url = d.meta.authorLogin.isEmpty() ? workUrl(workId)
                                                         : QStringLiteral("https://author.today/u/%1").arg(d.meta.authorLogin);
        const QString hint = m_ctx->auth->isLoggedIn()
            ? QStringLiteral("Автор открыл скачивание этой книги своим подписчикам.\n1. Наведите камеру телефона на код.\n"
                             "2. Подпишитесь на автора на сайте.\n3. Нажмите «Я подписался» — приложение обновит статус книги.")
            : QStringLiteral("Автор открыл скачивание этой книги своим подписчикам. Подпишитесь на автора на сайте "
                             "и войдите в приложении в тот же аккаунт.");
        if (Ui::showQr(parent, QStringLiteral("Скачивание — подписчикам"), d.meta.authors, url, hint,
                       m_ctx->auth->isLoggedIn() ? QStringLiteral("Я подписался") : QString())) {
            m_ctx->status->invalidate(workId);
            m_ctx->status->request({workId});
        }
        return;
    }
    case ActionState::SiteOnly:
        Ui::showQr(parent, QStringLiteral("Читать на сайте"), m_ctx->status->get(workId).meta.title, workUrl(workId),
                   QStringLiteral("Автор разрешает читать эту книгу только на сайте или в приложении author.today."));
        return;
    case ActionState::Unknown:
        emit openCard(workId);
        return;
    }
}

void BookActions::withFreshDetails(qint64 workId, std::function<void(const WorkDetails &)> then,
                                   std::function<void(const ApiError &)> failed)
{
    // Download rights can change at any time: ask the server right before downloading.
    m_ctx->client->get(QStringLiteral("/v1/work/%1/details").arg(workId), [then, failed](const QJsonDocument &doc, const ApiError *e) {
        if (e)
            failed(*e);
        else
            then(WorkDetails::fromJson(doc.object()));
    });
}

void BookActions::updateAll(const QList<qint64> &workIds, QWidget *parent)
{
    if (workIds.isEmpty())
        return;
    if (m_busy) {
        Ui::toast(parent, QStringLiteral("Подождите: скачивается другая книга"));
        return;
    }
    if (!Ui::confirm(parent, QStringLiteral("Обновить книги: %1. Место чтения в них начнётся сначала.").arg(workIds.size()),
                     QStringLiteral("Обновить все")))
        return;
    QPointer<QWidget> guard(parent);
    // Sequential: one download at a time. The chain clears itself at the end (no shared_ptr cycle).
    auto updated = std::make_shared<int>(0);
    auto next = std::make_shared<std::function<void(int)>>();
    std::weak_ptr<std::function<void(int)>> weakNext = next;
    *next = [this, workIds, guard, weakNext, updated](int i) {
        if (i >= workIds.size() || !guard) {
            m_updatingAll = false;
            if (guard)
                Ui::toast(guard, QStringLiteral("Обновлено книг: %1 из %2").arg(*updated).arg(workIds.size()));
            return;
        }
        // The pending download's callback owns the chain; the chain itself only points back weakly.
        auto self = weakNext.lock();
        download(workIds[i], guard, false, [self, updated, i](bool ok) {
            if (ok)
                ++*updated;
            if (self)
                (*self)(i + 1);
        });
    };
    m_updatingAll = true;
    // One connection check for the whole chain: offline, each book would wait out its own attempt.
    m_ctx->networkGate()([this, next, guard](bool online) {
        if (online) {
            (*next)(0);
            return;
        }
        m_updatingAll = false;
        if (guard)
            Ui::toast(guard, QStringLiteral("Нет подключения к интернету"));
    });
}

void BookActions::download(qint64 workId, QWidget *parent, bool open, std::function<void(bool)> then)
{
    if (m_updatingAll && !then) { // a tap between two books of «Обновить все» would knock one out
        Ui::toast(parent, QStringLiteral("Подождите: идёт обновление книг"));
        return;
    }
    if (m_busy) {
        // One download at a time; say so instead of ignoring the tap.
        Ui::toast(parent, workId == m_busyId ? QStringLiteral("Эта книга уже скачивается")
                          : m_updatingAll   ? QStringLiteral("Подождите: идёт обновление книг")
                                            : QStringLiteral("Подождите: скачивается другая книга"));
        if (then)
            then(false);
        return;
    }
    QPointer<QWidget> guard(parent);
    if (!device::networkConnected()) { // Wi-Fi may have been switched off by the firmware
        m_ctx->networkGate()([this, workId, guard, open, then](bool ok) {
            if (ok) {
                download(workId, guard, open, then);
                return;
            }
            if (guard)
                Ui::toast(guard, QStringLiteral("Нет подключения к интернету"));
            if (then)
                then(false);
        });
        return;
    }
    m_busy = true;
    m_busyId = workId;
    Ui::toast(parent, QStringLiteral("Скачиваю…"));
    withFreshDetails(workId, [this, workId, guard, open, then](const WorkDetails &details) {
        m_ctx->downloader->download(details, m_ctx->booksDir, m_ctx->settings->downloadFormat,
                                    [this, workId, guard, open, then](const QString &path, const ApiError *e) {
            m_busy = false;
            m_ctx->status->invalidate(workId);
            m_ctx->status->request({workId});
            m_ctx->emitStatusChanged(workId);
            if (e) {
                if (guard)
                    Ui::toast(guard, e->message);
                if (then)
                    then(false);
                return;
            }
            if (open && m_ctx->settings->openAfterDownload)
                device::openBook(path);
            else if (guard && !then)
                Ui::toast(guard, QStringLiteral("Книга скачана."));
            if (then)
                then(true);
        });
    }, [this, guard, then](const ApiError &e) {
        m_busy = false;
        if (guard)
            Ui::toast(guard, e.message);
        if (then)
            then(false);
    });
}
