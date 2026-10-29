#include "CatalogScreens.h"

#include "AppContext.h"
#include "Settings.h"
#include "Theme.h"
#include "WorkGrid.h"
#include "WorkList.h"
#include "ApiClient.h"
#include "SiteSearch.h"

#include <QHBoxLayout>
#include <QLineEdit>
#include <QJsonDocument>
#include <QLocale>
#include <QPointer>
#include <QStyle>
#include <QVBoxLayout>

#include <algorithm>

namespace {

void setDark(QPushButton *b, bool dark)
{
    b->setProperty("variant", dark ? "dark" : "normal");
    b->style()->unpolish(b);
    b->style()->polish(b);
}

QPushButton *chip(const QString &text, QWidget *parent)
{
    auto *b = new QPushButton(text, parent);
    b->setFont(Theme::smallFont());
    b->setFocusPolicy(Qt::NoFocus);
    return b;
}

QString captionOf(const QList<QPair<QString, QString>> &list, const QString &value)
{
    for (const auto &p : list)
        if (p.first == value)
            return p.second;
    return value;
}

// Ui::choose over a value list; returns the chosen value or `current` when cancelled.
QString pick(QWidget *parent, const QString &title, const QList<QPair<QString, QString>> &list, const QString &current)
{
    QStringList captions;
    for (const auto &p : list)
        captions << (p.first == current ? QStringLiteral("✓ ") + p.second : p.second);
    const int i = Ui::choose(parent, title, captions);
    return i >= 0 ? list[i].first : current;
}

// Chip in the search row: shares the width with its neighbours and clips long text.
QPushButton *extraChip(const QString &text, QWidget *parent)
{
    auto *b = new ElidedButton(text, parent);
    b->setFont(Theme::smallFont());
    b->setFocusPolicy(Qt::NoFocus);
    b->setStyleSheet(QStringLiteral("QPushButton { border-radius: %1px; padding: 0 %2px; }").arg(Theme::mm(3)).arg(Theme::mm(1.5)));
    return b;
}

} // namespace

CatalogScreen::CatalogScreen(AppContext *ctx, QWidget *parent)
    : Screen(parent)
    , m_ctx(ctx)
{
    auto *v = new QVBoxLayout(this);
    v->setContentsMargins(0, 0, 0, 0);
    auto *top = new TopBar(QStringLiteral("Каталог"), false, this);
    m_ctx->addHeaderButtons(top);
    v->addWidget(top);

    auto *body = new QVBoxLayout;
    body->setContentsMargins(Theme::mm(2), Theme::mm(1.5), Theme::mm(2), 0);
    body->setSpacing(Theme::mm(1));

    // The one search field of the app.
    m_input = new QLineEdit(this);
    m_input->setFont(Theme::bodyFont());
    m_input->setPlaceholderText(QStringLiteral("Название, автор или тег"));
    auto run = [this] {
        Ui::hideKeyboard(m_input);
        const QString text = m_input->text().trimmed();
        if (text.isEmpty())
            endSearch();
        else
            search(text);
    };
    connect(m_input, &QLineEdit::returnPressed, this, run);
    // The firmware keyboard may not send Return: an explicit button as well.
    auto *find = new QPushButton(Theme::icon(QStringLiteral("magnifying-glass")), QStringLiteral("Найти"), this);
    find->setFont(Theme::smallFont());
    find->setFocusPolicy(Qt::NoFocus);
    connect(find, &QPushButton::clicked, this, run);
    auto *inputRow = new QHBoxLayout;
    inputRow->addWidget(m_input, 1);
    inputRow->addWidget(find);
    body->addLayout(inputRow);

    // Browse mode: filter buttons and "Найдено: N · … Сбросить".
    m_filters = new QWidget(this);
    auto *chips = new QHBoxLayout(m_filters);
    chips->setContentsMargins(0, 0, 0, 0);
    chips->setSpacing(Theme::mm(1));
    m_genreBtn = chip(QStringLiteral("Жанр"), m_filters);
    m_sortBtn = chip(QStringLiteral("Сортировка"), m_filters);
    m_stateBtn = chip(QStringLiteral("Статус"), m_filters);
    m_accessBtn = chip(QStringLiteral("Доступ"), m_filters);
    m_dnlBtn = chip(QStringLiteral("Можно скачать"), m_filters);
    for (QPushButton *b : {m_genreBtn, m_sortBtn, m_stateBtn, m_accessBtn, m_dnlBtn})
        chips->addWidget(b, 1);
    body->addWidget(m_filters);
    connect(m_genreBtn, &QPushButton::clicked, this, [this] { emit pickGenre(m_query.genre); });
    connect(m_sortBtn, &QPushButton::clicked, this, [this] {
        const QString v = pick(this, QStringLiteral("Сортировка"), catalogSortings(), m_query.sorting);
        if (v != m_query.sorting) {
            m_query.sorting = v;
            reload();
        }
    });
    connect(m_stateBtn, &QPushButton::clicked, this, [this] {
        const QString v = pick(this, QStringLiteral("Статус"), catalogStates(), m_query.state);
        if (v != m_query.state) {
            m_query.state = v;
            reload();
        }
    });
    connect(m_accessBtn, &QPushButton::clicked, this, [this] {
        const QString v = pick(this, QStringLiteral("Доступ"), catalogAccesses(), m_query.access);
        if (v != m_query.access) {
            m_query.access = v;
            reload();
        }
    });
    connect(m_dnlBtn, &QPushButton::clicked, this, [this] {
        m_query.onlyDownloadable = !m_query.onlyDownloadable;
        reload();
    });

    m_countRow = new QWidget(this);
    auto *countRow = new QHBoxLayout(m_countRow);
    countRow->setContentsMargins(0, 0, 0, 0);
    m_found = new QLabel(m_countRow);
    m_found->setFont(Theme::smallFont());
    // One line, clipped: every extra line above the tiles pushes the tab bar off the screen.
    m_found->setSizePolicy(QSizePolicy::Ignored, QSizePolicy::Preferred);
    m_reset = chip(QStringLiteral("Сбросить"), m_countRow);
    connect(m_reset, &QPushButton::clicked, this, [this] {
        m_query = CatalogQuery();
        m_query.onlyDownloadable = m_ctx->settings->onlyDownloadable;
        m_query.hideAi = m_ctx->settings->hideAi;
        m_genreTitle.clear();
        reload();
    });
    countRow->addWidget(m_found, 1);
    countRow->addWidget(m_reset);
    body->addWidget(m_countRow);

    // Search mode: "Поиск: «…» ✕", then authors and the tag row.
    m_searchRow = new QWidget(this);
    auto *searchRow = new QHBoxLayout(m_searchRow);
    searchRow->setContentsMargins(0, 0, 0, 0);
    m_searchLabel = new QLabel(m_searchRow);
    m_searchLabel->setFont(Theme::smallFont());
    m_searchLabel->setSizePolicy(QSizePolicy::Ignored, QSizePolicy::Preferred);
    QPushButton *close = chip(QStringLiteral("✕ К каталогу"), m_searchRow);
    connect(close, &QPushButton::clicked, this, [this] {
        m_input->clear();
        endSearch();
    });
    searchRow->addWidget(m_searchLabel, 1);
    searchRow->addWidget(close);
    body->addWidget(m_searchRow);
    m_extras = new QWidget(this);
    m_extrasLayout = new QHBoxLayout(m_extras);
    m_extrasLayout->setContentsMargins(0, 0, 0, 0);
    m_extrasLayout->setSpacing(Theme::mm(1));
    body->addWidget(m_extras);

    m_grid = new WorkGrid(m_ctx, this);
    body->addWidget(m_grid, 1);
    v->addLayout(body, 1);

    m_query.onlyDownloadable = m_ctx->settings->onlyDownloadable;
    m_query.hideAi = m_ctx->settings->hideAi;
    m_hideAdult = m_ctx->settings->hideAdult;
    m_dnlSetting = m_ctx->settings->onlyDownloadable;
    showSearchMode(false);
    paintFilters(); // the list loads on the first visit: no catalog traffic at app start
}

CatalogScreen::~CatalogScreen() = default;

void CatalogScreen::pageKey(int direction)
{
    m_grid->flip(direction);
}

void CatalogScreen::shown()
{
    // Settings may have changed since the list was built (18+, AI, "only downloadable").
    const Settings *s = m_ctx->settings;
    bool changed = !m_list;
    if (m_hideAdult != s->hideAdult || m_query.hideAi != s->hideAi) {
        m_hideAdult = s->hideAdult;
        m_query.hideAi = s->hideAi;
        changed = true;
    }
    if (m_dnlSetting != s->onlyDownloadable) {
        m_dnlSetting = s->onlyDownloadable;
        m_query.onlyDownloadable = s->onlyDownloadable;
        changed = true;
    }
    if (changed) {
        if (m_searchText.isEmpty())
            reload();
        else
            search(m_searchText);
    }
}

void CatalogScreen::focusSearch()
{
    m_input->setFocus(); // the firmware keyboard opens
}

void CatalogScreen::setGenre(const QString &code, const QString &title)
{
    if (code == m_query.genre)
        return;
    m_query.genre = code;
    m_genreTitle = title;
    reload();
}

void CatalogScreen::setTag(const QString &tag)
{
    ++m_request; // answers of the search being replaced must not add rows any more
    clearExtras();
    m_input->clear();
    m_searchText.clear();
    showSearchMode(false);
    m_query.tag = tag;
    reload();
}

void CatalogScreen::showSearchMode(bool on)
{
    m_filters->setVisible(!on);
    m_countRow->setVisible(!on);
    m_searchRow->setVisible(on);
    m_extras->setVisible(on && m_extrasLayout->count() > 0);
}

void CatalogScreen::search(const QString &text)
{
    m_searchText = text;
    const int request = ++m_request;
    clearExtras();
    m_searchLabel->setText(QStringLiteral("Поиск «%1» · названия и авторы через сайт").arg(text));
    showSearchMode(true);
    const Settings *s = m_ctx->settings;
    m_grid->setList(nullptr);
    m_list.reset(new WorkList(siteSearchSource(m_ctx->client, text), s->hideAdult, s->hideAi));
    m_list->setNetworkGate(m_ctx->networkGate());
    m_grid->setList(m_list.get());
    // Authors and the tag row come after the network check, like the books.
    QPointer<CatalogScreen> self(this);
    m_ctx->networkGate()([self, text, request](bool ok) {
        if (!self || !ok || request != self->m_request)
            return;
        self->checkTag(text, request);
        self->loadAuthors(text, request);
    });
}

void CatalogScreen::endSearch()
{
    ++m_request;
    m_searchText.clear();
    clearExtras();
    showSearchMode(false);
    reload();
}

void CatalogScreen::clearExtras()
{
    while (QLayoutItem *item = m_extrasLayout->takeAt(0)) {
        if (QWidget *w = item->widget()) {
            w->hide();
            w->deleteLater(); // a row may be the button being clicked
        }
        delete item;
    }
    m_extras->hide();
}

// One catalog request with a page of 1: does a tag with exactly this name exist, and how big is it.
void CatalogScreen::checkTag(const QString &text, int request)
{
    CatalogQuery q;
    q.tag = text;
    q.onlyDownloadable = m_query.onlyDownloadable;
    q.hideAi = m_query.hideAi;
    QPointer<CatalogScreen> self(this);
    m_ctx->client->get(catalogPath(q, 1, 1), [self, text, request](const QJsonDocument &doc, const ApiError *e) {
        if (!self || e || request != self->m_request)
            return;
        const CatalogPage p = parseCatalogPage(doc.object());
        if (p.total <= 0)
            return;
        QPushButton *b = extraChip(QStringLiteral("#%1 · %2 ›").arg(text, QLocale(QLocale::Russian).toString(p.total)),
                                   self->m_extras);
        CatalogScreen *screen = self;
        QObject::connect(b, &QPushButton::clicked, screen, [screen, text] { screen->setTag(text); });
        // The tag goes first; if the authors already took all three places, the last one gives way.
        if (self->m_extrasLayout->count() >= 3) {
            if (QLayoutItem *last = self->m_extrasLayout->takeAt(self->m_extrasLayout->count() - 1)) {
                if (QWidget *w = last->widget()) {
                    w->hide();
                    w->deleteLater();
                }
                delete last;
            }
        }
        self->m_extrasLayout->insertWidget(0, b);
        self->m_extras->show();
    });
}

void CatalogScreen::loadAuthors(const QString &text, int request)
{
    QPointer<CatalogScreen> self(this);
    m_ctx->client->getRaw(siteAuthorsUrl(text), [self, request](const HttpResponse &r, const ApiError *e) {
        if (!self || e || request != self->m_request)
            return;
        bool ok = false;
        const QList<SiteAuthor> authors = parseSiteAuthors(r.body, &ok);
        if (!ok)
            return;
        AppContext *ctx = self->m_ctx;
        // Three chips in the row at most, the tag (if any) included.
        const int room = qMax(0, 3 - self->m_extrasLayout->count());
        for (const SiteAuthor &a : authors.mid(0, room)) {
            QPushButton *b = extraChip(QStringLiteral("%1 ›").arg(a.name), self->m_extras);
            const QString login = a.login, name = a.name;
            QObject::connect(b, &QPushButton::clicked, ctx, [ctx, login, name] { emit ctx->openAuthor(login, name); });
            self->m_extrasLayout->addWidget(b);
        }
        if (!authors.isEmpty())
            self->m_extras->show();
    }, {{"User-Agent", siteUserAgent()}});
}

void CatalogScreen::reload()
{
    m_grid->setList(nullptr);
    m_list.reset(new WorkList(catalogSource(m_ctx->client, m_query), m_hideAdult, m_query.hideAi));
    m_list->setNetworkGate(m_ctx->networkGate());
    connect(m_list.get(), &WorkList::changed, this, &CatalogScreen::paintFound);
    paintFilters(); // reload() is called only when the query changed
    m_grid->setList(m_list.get());
}

void CatalogScreen::paintFilters()
{
    // Only when the query changes: repolishing the chips repaints the row on e-ink.
    const CatalogQuery d;
    setDark(m_genreBtn, !m_query.genre.isEmpty());
    setDark(m_sortBtn, m_query.sorting != d.sorting);
    setDark(m_stateBtn, m_query.state != d.state);
    setDark(m_accessBtn, m_query.access != d.access);
    setDark(m_dnlBtn, m_query.onlyDownloadable);
    m_reset->setVisible(!m_query.tag.isEmpty() || !m_query.genre.isEmpty() || m_query.sorting != d.sorting
                        || m_query.state != d.state || m_query.access != d.access
                        || m_query.onlyDownloadable != m_ctx->settings->onlyDownloadable);
    paintFound();
}

void CatalogScreen::paintFound()
{
    const CatalogQuery d;
    QStringList parts;
    if (m_list && m_list->total() >= 0)
        parts << QStringLiteral("Найдено: %1").arg(QLocale(QLocale::Russian).toString(m_list->total()));
    if (!m_query.tag.isEmpty())
        parts << QStringLiteral("тег «%1»").arg(m_query.tag);
    if (!m_genreTitle.isEmpty())
        parts << m_genreTitle;
    parts << captionOf(catalogSortings(), m_query.sorting);
    if (m_query.state != d.state)
        parts << captionOf(catalogStates(), m_query.state);
    if (m_query.access != d.access)
        parts << captionOf(catalogAccesses(), m_query.access);
    const QString text = parts.join(QStringLiteral(" · "));
    if (m_found->text() != text)
        m_found->setText(text);
}

// ---------- GenreScreen ----------

GenreScreen::GenreScreen(const QList<Genre> &all, const QString &currentCode, QWidget *parent)
    : Screen(parent)
    , m_all(all)
    , m_current(currentCode)
{
    auto *v = new QVBoxLayout(this);
    v->setContentsMargins(0, 0, 0, 0);
    m_top = new TopBar(QStringLiteral("Жанр"), true, this);
    connect(m_top, &TopBar::back, this, [this] {
        if (m_parent)
            showLevel(0);
        else
            emit back();
    });
    v->addWidget(m_top);
    m_pager = new Pager(1, 10, this);
    m_pager->setContentsMargins(Theme::mm(3), Theme::mm(1), Theme::mm(3), 0);
    m_pager->setEmptyText(QStringLiteral("Список жанров ещё не загружен. Проверьте сеть и откройте снова."));
    m_pager->setFactory([this](int i) {
        auto *b = new QPushButton(m_rows[i].caption);
        b->setFont(Theme::bodyFont());
        b->setFocusPolicy(Qt::NoFocus);
        b->setStyleSheet(QStringLiteral("QPushButton { text-align: left; padding-left: %1px; }").arg(Theme::mm(2)));
        QObject::connect(b, &QPushButton::clicked, this, [this, i] {
            const Row r = m_rows[i];
            if (r.code.startsWith(QLatin1Char('#')))
                showLevel(r.code.mid(1).toInt());
            else
                emit chosen(r.code, r.title);
        });
        return b;
    });
    v->addWidget(m_pager, 1);
    // Open on the level that holds the current genre.
    int start = 0;
    for (const Genre &g : m_all)
        if (g.code == currentCode && g.parentId)
            start = g.parentId;
    showLevel(start);
}

void GenreScreen::showLevel(int parentId)
{
    m_parent = parentId;
    m_rows.clear();
    auto mark = [this](const QString &code, const QString &title) {
        return code == m_current ? QStringLiteral("✓ ") + title : title;
    };
    if (parentId == 0) {
        m_top->setTitle(QStringLiteral("Жанр"));
        m_rows << Row{QString(), mark(QString(), QStringLiteral("Все жанры")), QString()};
        for (const Genre &g : m_all) {
            if (g.parentId)
                continue;
            const bool hasChildren = std::any_of(m_all.begin(), m_all.end(), [&g](const Genre &c) { return c.parentId == g.id; });
            if (hasChildren)
                m_rows << Row{QStringLiteral("#%1").arg(g.id), mark(g.code, g.title) + QStringLiteral("  ›"), g.title};
            else
                m_rows << Row{g.code, mark(g.code, g.title), g.title};
        }
    } else {
        for (const Genre &g : m_all) {
            if (g.id == parentId) {
                m_top->setTitle(g.title);
                m_rows << Row{g.code, mark(g.code, QStringLiteral("Весь жанр «%1»").arg(g.title)), g.title};
            }
        }
        for (const Genre &g : m_all)
            if (g.parentId == parentId)
                m_rows << Row{g.code, mark(g.code, g.title), g.title};
    }
    m_pager->setCount(m_rows.size());
}
