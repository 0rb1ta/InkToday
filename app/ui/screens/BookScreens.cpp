#include "BookScreens.h"

#include "ApiClient.h"
#include "AppContext.h"
#include "AuthService.h"
#include "BookActions.h"
#include "CoverCache.h"
#include "DownloadIndex.h"
#include "LibraryActions.h"
#include "LibraryStore.h"
#include "ReaderLibrary.h"
#include "ReadingPosition.h"
#include "Settings.h"
#include "StatusCache.h"
#include "Theme.h"

#include <QGridLayout>
#include <QHBoxLayout>
#include <QJsonArray>
#include <QLabel>
#include <QPointer>
#include <QPushButton>
#include <QTextDocumentFragment>
#include <QUrlQuery>
#include <QVBoxLayout>

namespace {

const QStringList kShelfStates{QStringLiteral("Reading"), QStringLiteral("Saved"), QStringLiteral("Finished"),
                               QStringLiteral("Disliked"), QStringLiteral("None")};
const QStringList kShelfNames{QStringLiteral("Читаю"), QStringLiteral("Отложено"), QStringLiteral("Прочитано"),
                              QStringLiteral("Не понравилось"), QStringLiteral("Убрать из библиотеки")};

QString plain(const QString &html)
{
    return QTextDocumentFragment::fromHtml(html).toPlainText().trimmed();
}

QString volume(qint64 chars)
{
    if (chars <= 0)
        return QString();
    return QStringLiteral("%1 тыс. знаков · ~%2 а. л.").arg(chars / 1000).arg(qMax<qint64>(1, (chars + 20000) / 40000));
}

QString shelfName(const QString &state)
{
    const int i = kShelfStates.indexOf(state);
    return i >= 0 && state != QLatin1String("None") ? kShelfNames[i] : QString();
}

QLabel *line(const QString &icon, const QString &text, QWidget *parent)
{
    auto *l = new QLabel(QStringLiteral("%1  %2").arg(icon, text), parent);
    l->setFont(Theme::smallFont());
    l->setWordWrap(true);
    return l;
}

QWidget *backBar(Screen *screen, const QString &title, std::function<void()> onBack, TopBar **out = nullptr,
                 AppContext *ctx = nullptr)
{
    auto *top = new TopBar(title, true, screen);
    if (ctx)
        ctx->addHeaderButtons(top);
    QObject::connect(top, &TopBar::back, screen, [onBack] { onBack(); });
    if (out)
        *out = top;
    return top;
}

} // namespace

// ---------- BookScreen ----------

BookScreen::BookScreen(AppContext *ctx, BookActions *actions, qint64 workId, QWidget *parent)
    : Screen(parent)
    , m_ctx(ctx)
    , m_actions(actions)
    , m_id(workId)
{
    auto *v = new QVBoxLayout(this);
    v->setContentsMargins(0, 0, 0, 0);
    TopBar *top = nullptr;
    v->addWidget(backBar(this, QString(), [this] { emit back(); }, &top, m_ctx));
    QPushButton *share = top->addIconButton(QStringLiteral("share-nodes"));
    connect(share, &QPushButton::clicked, this, [this] {
        Ui::showQr(this, QStringLiteral("Поделиться"), m_details.meta.title,
                   QStringLiteral("https://author.today/work/%1").arg(m_id), QStringLiteral("Откройте книгу на телефоне."));
    });
    m_header = new QWidget(this);
    m_headerLayout = new QVBoxLayout(m_header);
    m_headerLayout->setContentsMargins(Theme::mm(2.5), Theme::mm(2), Theme::mm(2.5), 0);
    v->addWidget(m_header);
    m_pager = new Pager(1, 7, this);
    m_pager->setContentsMargins(Theme::mm(2.5), Theme::mm(1), Theme::mm(2.5), 0);
    m_pager->setFactory([this](int i) { return chapterRow(i); });
    v->addWidget(m_pager, 1);

    connect(m_ctx, &AppContext::statusChanged, this, [this](qint64 id) {
        if (id == m_id)
            render();
    });
    m_ctx->status->invalidate(m_id); // always show the latest rights and chapters
    m_ctx->status->requestNow(m_id);
    connect(m_ctx->status, &StatusCache::failed, this, [this](qint64 id, const QString &message) {
        if (id == m_id && !m_ctx->status->has(m_id)) {
            m_loadError = message;
            render();
        }
    });
    render(); // asks for the details first in the queue
}

void BookScreen::render()
{
    // A fresh header widget each time (the old code deleted nested layouts twice and crashed).
    auto *fresh = new QWidget(this);
    auto *outer = static_cast<QVBoxLayout *>(layout());
    outer->replaceWidget(m_header, fresh);
    m_header->hide();
    m_header->deleteLater();
    m_header = fresh;
    m_headerLayout = new QVBoxLayout(m_header);
    m_headerLayout->setContentsMargins(Theme::mm(2.5), Theme::mm(2), Theme::mm(2.5), 0);
    if (!m_ctx->status->has(m_id)) {
        // Also after "Очистить кэш" or logout while the card was open, or a failed first try.
        auto *wait = new QLabel(m_loadError.isEmpty() ? QStringLiteral("Загружаю карточку книги…") : m_loadError, m_header);
        wait->setAlignment(Qt::AlignCenter);
        wait->setWordWrap(true);
        m_headerLayout->addWidget(wait);
        if (m_loadError.isEmpty()) {
            m_ctx->status->requestNow(m_id);
        } else {
            auto *retry = new QPushButton(QStringLiteral("Повторить"), m_header);
            connect(retry, &QPushButton::clicked, this, [this] {
                m_loadError.clear();
                render();
            });
            m_headerLayout->addWidget(retry);
        }
        m_pager->setCount(0);
        return;
    }
    m_details = m_ctx->status->get(m_id);
    const WorkMeta &w = m_details.meta;

    auto *top = new QHBoxLayout;
    top->setSpacing(Theme::mm(2.5));
    auto *cover = new QLabel(m_header);
    cover->setFixedSize(Theme::mm(28), Theme::mm(38));
    cover->setStyleSheet(QStringLiteral("background: #e6e6e6; border: 1px solid #555;"));
    if (m_ctx->settings->loadCovers && !w.coverUrl.isEmpty()) {
        QPointer<QLabel> guard(cover);
        m_ctx->covers->get(w.coverUrl, [guard](const QImage &img) {
            if (guard && !img.isNull())
                guard->setPixmap(QPixmap::fromImage(img.scaled(guard->size(), Qt::KeepAspectRatio, Qt::SmoothTransformation)));
        });
    }
    top->addWidget(cover, 0, Qt::AlignTop);
    auto *info = new QVBoxLayout;
    info->setSpacing(Theme::mm(0.6)); // dense: the card has to leave room for the chapter list
    auto *title = new ElidedLabel(3, m_header);
    title->setFont(Theme::titleFont());
    title->setReserveAllLines(false); // a one-line title must not leave two empty lines under it
    title->setFullText(w.title);
    info->addWidget(title);
    if (w.authorLogin.isEmpty()) {
        auto *author = new QLabel(w.authors, m_header);
        author->setFont(Theme::bodyFont());
        author->setWordWrap(true);
        info->addWidget(author);
    } else {
        // Tap: all books of the first author (site page /u/<login>/works).
        auto *author = new ElidedButton(w.authors + QStringLiteral(" ›"), m_header); // long co-author lists end with "…"
        author->setFlat(true);
        author->setFont(Theme::bodyFont());
        author->setFocusPolicy(Qt::NoFocus);
        author->setStyleSheet(QStringLiteral("QPushButton { border: none; text-align: left; padding: 0; min-height: %1px; }")
                                  .arg(Theme::mm(6)));
        const QString login = w.authorLogin;
        const QString name = w.authors.section(QStringLiteral(", "), 0, 0);
        connect(author, &QPushButton::clicked, m_ctx, [this, login, name] { emit m_ctx->openAuthor(login, name); });
        info->addWidget(author);
    }
    if (w.seriesId) {
        auto *series = new ElidedButton(QStringLiteral("Цикл «%1», книга %2 ›").arg(w.seriesTitle).arg(m_details.seriesWorkNumber), m_header);
        series->setFlat(true);
        series->setFont(Theme::smallFont());
        series->setStyleSheet(QStringLiteral("border: none; text-align: left; padding: 0;"));
        series->setMinimumHeight(Theme::mm(6));
        connect(series, &QPushButton::clicked, this, [this] { emit openSeries(m_details); });
        info->addWidget(series);
    }
    const QDateTime updated = w.lastUpdateTime.isValid() ? w.lastUpdateTime : w.lastModificationTime;
    info->addWidget(line(QStringLiteral("✎"), QStringLiteral("%1 · обновлено %2")
                                                 .arg(w.isFinished ? QStringLiteral("Завершено") : QStringLiteral("В процессе"))
                                                 .arg(updated.toLocalTime().toString(QStringLiteral("dd.MM.yyyy"))), m_header));
    if (!volume(w.textLength).isEmpty())
        info->addWidget(line(QStringLiteral("≡"), volume(w.textLength), m_header));
    const QString genres = m_ctx->genreNames(w.genreIds);
    if (!genres.isEmpty())
        info->addWidget(line(QStringLiteral("#"), genres, m_header));
    info->addWidget(line(QStringLiteral("♥"), QStringLiteral("%1 · %2").arg(w.likeCount)
                                                  .arg(w.price > 0 ? QStringLiteral("%1 ₽").arg(w.price) : QStringLiteral("Бесплатно")), m_header));
    info->addStretch(1);
    top->addLayout(info, 1);
    m_headerLayout->addLayout(top);

    // Main action.
    const ActionState state = m_ctx->stateFor(w);
    auto *main = new ActionButton(m_header);
    main->setState(state);
    main->setMinimumHeight(Theme::mm(10));
    main->setFont(Theme::bodyFont());
    if (state == ActionState::Update) {
        const int n = newChapters(m_ctx->index->get(m_id), m_details);
        main->setText(n > 0 ? QStringLiteral("Обновить и читать (+%1 гл.)").arg(n) : QStringLiteral("Обновить и читать"));
    } else if (state == ActionState::Download) {
        main->setText(QStringLiteral("Скачать и читать"));
    } else if (state == ActionState::SiteOnly) {
        main->setText(QStringLiteral("Скачивание закрыто автором"));
    } else if (state == ActionState::Subscribers) {
        main->setText(QStringLiteral("Скачивание — подписчикам автора"));
    }
    connect(main, &QPushButton::clicked, this, [this, state] { m_actions->run(m_id, state, this); });
    m_headerLayout->addWidget(main);
    if (state == ActionState::Subscribers) {
        auto *why = new ElidedLabel(2, m_header); // two lines at most: the card must keep the tab bar on screen
        why->setFullText(QStringLiteral("Автор открыл скачивание своим подписчикам. Подписаться можно на сайте — "
                                        "нажмите кнопку выше, появится QR-код."));
        why->setFont(Theme::smallFont());
        why->setStyleSheet(QStringLiteral("border: 1px dashed #000; padding: 4px;"));
        m_headerLayout->addWidget(why);
    }
    if (state == ActionState::SiteOnly) {
        auto *why = new ElidedLabel(2, m_header); // the server's text has no length limit
        why->setFullText(m_details.downloadErrorMessage.isEmpty()
                             ? QStringLiteral("Автор разрешает читать эту книгу только на сайте или в приложении author.today.")
                             : m_details.downloadErrorMessage);
        why->setFont(Theme::smallFont());
        why->setStyleSheet(QStringLiteral("border: 1px dashed #000; padding: 4px;"));
        m_headerLayout->addWidget(why);
    }

    // Where the reader stopped: in the reader's own library and on the site (downloaded books).
    if (m_ctx->index->contains(m_id) && !m_ctx->index->fileMissing(m_id) && !m_details.chapters.isEmpty()) {
        const QString path = m_ctx->index->get(m_id).path;
        const ChapterMap map = mapChapters(readEpubToc(path), m_details.chapters, readEpubSpineCount(path));
        const ReaderPosition reader = m_ctx->readerLibrary ? m_ctx->readerLibrary->positionOf(path) : ReaderPosition();
        const PositionLines lines = describePositions(reader, map, m_details, m_ctx->settings->openAtSiteChapter);
        // Two lines at most (screen height): when the site is further, the hint replaces the site line.
        for (const QString &text : {lines.reader, lines.hint.isEmpty() ? lines.site : lines.hint}) {
            if (text.isEmpty())
                continue;
            auto *l = new ElidedLabel(1, m_header); // one line each: the card must keep the tab bar on screen
            l->setFont(Theme::smallFont());
            l->setFullText(text);
            l->setSizePolicy(QSizePolicy::Ignored, QSizePolicy::Fixed);
            m_headerLayout->addWidget(l);
        }
    }

    // Library shelf + similar.
    auto *row = new QHBoxLayout;
    QString current;
    for (const WorkMeta &lw : m_ctx->library->works())
        if (lw.id == m_id)
            current = shelfName(lw.libraryState);
    auto *shelf = new QPushButton(Theme::icon(current.isEmpty() ? QStringLiteral("regular:bookmark") : QStringLiteral("bookmark")),
                                  current.isEmpty() ? QStringLiteral("В библиотеку") : QStringLiteral("В библиотеке: %1").arg(current),
                                  m_header);
    auto *similar = new QPushButton(Theme::icon(QStringLiteral("wand-magic-sparkles")), QStringLiteral("Похожие"), m_header);
    connect(shelf, &QPushButton::clicked, this, &BookScreen::chooseShelf);
    connect(similar, &QPushButton::clicked, this, &BookScreen::showSimilar);
    row->addWidget(shelf, 1);
    row->addWidget(similar, 1);
    m_headerLayout->addLayout(row);

    // Tags: tap opens the catalog with that tag (they match only by exact name, so this is the
    // easy way to find them). One row only: the card already fills the screen, and a taller
    // header pushes the tab bar off the bottom. Three tags, the rest behind "ещё N".
    if (!m_details.tags.isEmpty()) {
        auto *tags = new QHBoxLayout;
        tags->setSpacing(Theme::mm(1));
        const QStringList all = m_details.tags;
        const int shown = all.size() > 4 ? 3 : all.size();
        auto tagButton = [this](const QString &text) {
            auto *b = new ElidedButton(text, m_header); // long tags end with "…"
            b->setFont(Theme::smallFont());
            b->setFocusPolicy(Qt::NoFocus);
            b->setStyleSheet(QStringLiteral("QPushButton { min-height: %1px; border-radius: %2px; padding: 0 %3px; }")
                                 .arg(Theme::mm(6.5)).arg(Theme::mm(3)).arg(Theme::mm(1)));
            return b;
        };
        for (int i = 0; i < shown; ++i) {
            QPushButton *b = tagButton(QStringLiteral("#") + all[i]);
            const QString tag = all[i];
            connect(b, &QPushButton::clicked, m_ctx, [this, tag] { emit m_ctx->openTag(tag); });
            tags->addWidget(b, 1);
        }
        if (shown < all.size()) {
            QPushButton *more = tagButton(QStringLiteral("ещё %1 ›").arg(all.size() - shown));
            connect(more, &QPushButton::clicked, this, [this, all] {
                const int i = Ui::choose(this, QStringLiteral("Теги книги"), all);
                if (i >= 0)
                    emit m_ctx->openTag(all[i]);
            });
            tags->addWidget(more, 1);
        }
        m_headerLayout->addLayout(tags);
    }

    m_pager->setEmptyText(QString());
    m_pager->setCount(1 + m_details.chapters.size()); // annotation + chapters
}

QWidget *BookScreen::chapterRow(int index)
{
    if (index == 0) {
        auto *w = new QWidget;
        auto *v = new QVBoxLayout(w);
        v->setContentsMargins(0, 0, 0, 0);
        auto *h = new QLabel(QStringLiteral("АННОТАЦИЯ"), w);
        h->setFont(Theme::smallFont());
        auto *text = new ElidedLabel(3, w);
        text->setFont(Theme::smallFont());
        const QString annotation = plain(m_details.annotation);
        text->setFullText(annotation.isEmpty() ? QStringLiteral("Аннотации нет.") : annotation);
        v->addWidget(h);
        v->addWidget(text);
        if (!annotation.isEmpty()) {
            auto *more = new QPushButton(QStringLiteral("Читать аннотацию полностью ›"), w);
            more->setFlat(true);
            more->setFont(Theme::smallFont());
            more->setStyleSheet(QStringLiteral("border: none; text-align: left;"));
            connect(more, &QPushButton::clicked, this, [this, annotation] { emit openText(m_details.meta.title, annotation); });
            v->addWidget(more);
        }
        return w;
    }
    // Chapters newest first.
    const int i = m_details.chapters.size() - index;
    const Chapter &c = m_details.chapters.value(i);
    const DownloadEntry e = m_ctx->index->get(m_id);
    const bool isNew = m_ctx->index->contains(m_id) && i >= e.chapterCount;
    auto *w = new QWidget;
    w->setStyleSheet(QStringLiteral("border-bottom: 1px solid #bbb;"));
    auto *h = new QHBoxLayout(w);
    h->setContentsMargins(0, 0, 0, 0);
    auto *t = new ElidedLabel(1, w); // one line: long chapter titles end with "…"
    QFont f = Theme::smallFont();
    f.setBold(isNew);
    t->setFont(f);
    t->setFullText((isNew ? QStringLiteral("● ") : QString()) + c.title);
    t->setSizePolicy(QSizePolicy::Ignored, QSizePolicy::Fixed);
    auto *d = new QLabel(c.publishTime.toLocalTime().toString(QStringLiteral("dd.MM")), w);
    d->setFont(Theme::smallFont());
    if (!c.isAvailable || c.isDraft)
        t->setProperty("role", "muted");
    h->addWidget(t, 1);
    h->addWidget(d);
    return w;
}

void BookScreen::chooseShelf()
{
    if (!m_ctx->auth->isLoggedIn()) {
        Ui::toast(this, QStringLiteral("Войдите, чтобы вести библиотеку."));
        return;
    }
    const int i = Ui::choose(this, QStringLiteral("Полка в библиотеке"), kShelfNames);
    if (i < 0)
        return;
    QPointer<BookScreen> guard(this);
    const WorkMeta meta = m_details.meta;
    m_ctx->networkGate()([this, guard, i, meta](bool online) {
        if (!guard)
            return;
        if (!online) {
            Ui::toast(this, QStringLiteral("Нет подключения к интернету"));
            return;
        }
        m_ctx->libraryActions->setState(m_id, kShelfStates[i], [this, guard, i, meta](const ApiError *e) {
            if (!guard)
                return;
            if (e) {
                Ui::toast(this, e->message);
                return;
            }
            // A book added from the card is not in the cached library yet: insert it.
            bool present = false;
            for (const WorkMeta &w : m_ctx->library->works())
                present |= w.id == meta.id;
            if (!present && kShelfStates[i] != QLatin1String("None")) {
                WorkMeta added = meta;
                added.libraryState = kShelfStates[i];
                m_ctx->library->applyDiff({added}, {}, m_ctx->library->lastSyncTime());
            }
            emit m_ctx->libraryChanged();
            Ui::toast(this, i == 4 ? QStringLiteral("Убрано из библиотеки") : QStringLiteral("На полке «%1»").arg(kShelfNames[i]));
            render();
        });
    });
}

void BookScreen::showSimilar()
{
    if (m_similarPending)
        return; // a double tap opened the list twice
    m_similarPending = true;
    Ui::toast(this, QStringLiteral("Ищу похожие…"));
    QPointer<BookScreen> guard(this);
    m_ctx->networkGate()([this, guard](bool online) {
        if (!guard)
            return;
        if (!online) {
            m_similarPending = false;
            Ui::toast(this, QStringLiteral("Нет подключения к интернету"));
            return;
        }
        m_ctx->client->get(QStringLiteral("/v1/work/%1/recommendations?count=8").arg(m_id), [this, guard](const QJsonDocument &doc, const ApiError *e) {
            if (!guard)
                return;
            m_similarPending = false;
            if (e) {
                Ui::toast(this, e->message);
                return;
            }
            QList<WorkMeta> works;
            for (const QJsonValue &v : doc.array())
                works << WorkMeta::fromJson(v.toObject());
            emit openShelf(QStringLiteral("Похожие"), works);
        });
    });
}

// ---------- SeriesScreen ----------

SeriesScreen::SeriesScreen(AppContext *ctx, const WorkDetails &from, QWidget *parent)
    : Screen(parent)
    , m_ctx(ctx)
    , m_from(from)
{
    auto *v = new QVBoxLayout(this);
    v->setContentsMargins(0, 0, 0, 0);
    v->addWidget(backBar(this, QStringLiteral("Цикл"), [this] { emit back(); }, nullptr, m_ctx));
    auto *box = new QWidget(this);
    auto *bv = new QVBoxLayout(box);
    bv->setContentsMargins(Theme::mm(2.5), Theme::mm(2), Theme::mm(2.5), 0);
    auto *title = new QLabel(from.meta.seriesTitle, box);
    title->setFont(Theme::titleFont());
    title->setWordWrap(true);
    auto *meta = new QLabel(QStringLiteral("%1 · книг: %2").arg(from.meta.authors).arg(from.seriesWorkIds.size()), box);
    meta->setFont(Theme::smallFont());
    bv->addWidget(title);
    bv->addWidget(meta);
    if (from.seriesPurchaseIsAllowed) {
        auto *buy = new QPushButton(Theme::icon(QStringLiteral("qrcode")), QStringLiteral("Купить весь цикл на сайте"), box);
        connect(buy, &QPushButton::clicked, this, [this] {
            Ui::showQr(this, QStringLiteral("Купить цикл на сайте"), m_from.meta.seriesTitle,
                       QStringLiteral("https://author.today/work/series/%1").arg(m_from.meta.seriesId),
                       QStringLiteral("Оплатите на телефоне, затем нажмите «Синхронизировать» на главной."));
        });
        bv->addWidget(buy);
    }
    v->addWidget(box);
    m_pager = new Pager(4, 2, this);
    m_pager->setContentsMargins(Theme::mm(2.5), Theme::mm(1), Theme::mm(2.5), 0);
    m_pager->setEmptyText(from.seriesWorkIds.isEmpty() ? QStringLiteral("В цикле пока нет книг.")
                                                       : QStringLiteral("Загружаю книги цикла…"));
    m_pager->setFactory([this](int i) { return row(i); });
    v->addWidget(m_pager, 1);
    for (int i = 0; i < from.seriesWorkIds.size(); ++i) {
        WorkMeta placeholder;
        placeholder.id = from.seriesWorkIds[i];
        m_works << placeholder;
    }
    // Tiles update themselves (makeTile); no whole-page rebuild per status.
    load(0);
}

void SeriesScreen::load(int from)
{
    if (from >= m_from.seriesWorkIds.size())
        return;
    QUrlQuery q;
    const int to = qMin(from + 20, m_from.seriesWorkIds.size());
    for (int i = from; i < to; ++i)
        q.addQueryItem(QStringLiteral("ids[%1]").arg(i - from), QString::number(m_from.seriesWorkIds[i]));
    QPointer<SeriesScreen> guard(this);
    const QString url = QStringLiteral("/v1/work/meta-info?") + q.toString(QUrl::FullyEncoded);
    m_ctx->networkGate()([this, guard, url, from, to](bool online) {
        if (!guard)
            return;
        if (!online) {
            m_pager->setCount(m_works.size()); // what is known so far, instead of loading forever
            return;
        }
        m_ctx->client->get(url,
                           [this, guard, from, to](const QJsonDocument &doc, const ApiError *e) {
            if (!guard)
                return;
            if (!e) {
                for (const QJsonValue &v : doc.array()) {
                    const QJsonObject o = v.toObject();
                    const WorkMeta w = WorkMeta::fromJson(o.value(QStringLiteral("data")).toObject());
                    const int pos = m_from.seriesWorkIds.indexOf(qint64(o.value(QStringLiteral("id")).toDouble()));
                    if (pos >= 0 && w.id)
                        m_works[pos] = w;
                }
                m_ctx->requestStatus(m_works.mid(from, to - from));
            }
            m_pager->setCount(m_works.size());
            load(to);
        });
    });
}

QWidget *SeriesScreen::row(int index)
{
    // Tiles like everywhere else; the badge carries the number in the series (● = the book we came from).
    const WorkMeta &w = m_works[index];
    const QString badge = QStringLiteral("%1%2").arg(index + 1).arg(w.id == m_from.meta.id ? QStringLiteral(" ●") : QString());
    return m_ctx->makeTile(w, nullptr, badge);
}

// ---------- TextScreen ----------

TextScreen::TextScreen(const QString &title, const QString &text, QWidget *parent)
    : Screen(parent)
{
    auto *v = new QVBoxLayout(this);
    v->setContentsMargins(0, 0, 0, 0);
    v->addWidget(backBar(this, title, [this] { emit back(); }));
    // ~1200 characters per page, split at paragraph boundaries.
    QString page;
    for (const QString &para : text.split(QLatin1Char('\n'), QString::SkipEmptyParts)) {
        if (!page.isEmpty() && page.size() + para.size() > 1200) {
            m_pages << page;
            page.clear();
        }
        page += para.trimmed() + QStringLiteral("\n\n");
    }
    if (!page.isEmpty())
        m_pages << page;
    m_pager = new Pager(1, 1, this);
    m_pager->setContentsMargins(Theme::mm(4), Theme::mm(3), Theme::mm(4), 0);
    m_pager->setFactory([this](int i) {
        auto *l = new QLabel(m_pages[i]);
        l->setFont(Theme::bodyFont());
        l->setWordWrap(true);
        l->setAlignment(Qt::AlignTop | Qt::AlignLeft);
        return l;
    });
    v->addWidget(m_pager, 1);
    m_pager->setCount(m_pages.size());
}
