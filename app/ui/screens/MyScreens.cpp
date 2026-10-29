#include "MyScreens.h"

#include "AppContext.h"
#include "AuthService.h"
#include "BookActions.h"
#include "CoverCache.h"
#include "DesktopIcon.h"
#include "DownloadIndex.h"
#include "HomeFeed.h"
#include "LibraryStore.h"
#include "PageMath.h"
#include "Paths.h"
#include "Settings.h"
#include "StatusCache.h"
#include "Theme.h"

#include <QFile>
#include <QFileInfo>
#include <QFontMetrics>
#include <QGridLayout>
#include <QHBoxLayout>
#include <QLabel>
#include <QPainter>
#include <QPushButton>
#include <QResizeEvent>
#include <QStyle>
#include <QTimer>
#include <QVBoxLayout>

namespace {

QPushButton *chip(const QString &text, QWidget *parent)
{
    auto *b = new QPushButton(text, parent);
    b->setFont(Theme::smallFont());
    b->setMinimumHeight(Theme::mm(8));
    return b;
}

void setDark(QPushButton *b, bool dark)
{
    b->setProperty("variant", dark ? "dark" : "normal");
    b->style()->unpolish(b);
    b->style()->polish(b);
}

QLabel *wrapped(const QString &text, const QFont &font, QWidget *parent)
{
    auto *l = new QLabel(text, parent);
    l->setFont(font);
    l->setWordWrap(true);
    return l;
}

} // namespace

// ---------- LibraryScreen ----------

LibraryScreen::LibraryScreen(AppContext *ctx, QWidget *parent)
    : Screen(parent)
    , m_ctx(ctx)
{
    auto *v = new QVBoxLayout(this);
    v->setContentsMargins(0, 0, 0, 0);
    auto *top = new TopBar(QStringLiteral("Моя библиотека"), false, this);
    ctx->addHeaderButtons(top);
    v->addWidget(top);
    auto *filters = new QWidget(this);
    auto *h = new QHBoxLayout(filters);
    h->setContentsMargins(Theme::mm(2), Theme::mm(1.5), Theme::mm(2), 0);
    h->setSpacing(Theme::mm(1));
    const QList<QPair<QString, QString>> shelves{{QString(), QStringLiteral("Все")},
                                                 {QStringLiteral("Reading"), QStringLiteral("Читаю")},
                                                 {QStringLiteral("Saved"), QStringLiteral("Отложено")},
                                                 {QStringLiteral("Finished"), QStringLiteral("Прочитано")},
                                                 {QStringLiteral("Disliked"), QStringLiteral("Не понр.")}};
    for (const auto &s : shelves) {
        QPushButton *b = chip(s.second, filters);
        const QString key = s.first;
        connect(b, &QPushButton::clicked, this, [this, key] {
            m_shelf = key;
            rebuild();
        });
        h->addWidget(b, 1);
        m_filters << b;
        b->setProperty("shelf", key);
    }
    v->addWidget(filters);
    auto *row2 = new QWidget(this);
    auto *h2 = new QHBoxLayout(row2);
    h2->setContentsMargins(Theme::mm(2), Theme::mm(1), Theme::mm(2), 0);
    m_downloadedToggle = chip(QStringLiteral("Только скачанные"), row2);
    connect(m_downloadedToggle, &QPushButton::clicked, this, [this] {
        m_onlyDownloaded = !m_onlyDownloaded;
        rebuild();
    });
    h2->addWidget(m_downloadedToggle);
    h2->addStretch(1);
    v->addWidget(row2);
    m_login = new QPushButton(Theme::icon(QStringLiteral("right-to-bracket")), QStringLiteral("Войти"), this);
    m_login->setMinimumHeight(Theme::mm(10));
    connect(m_login, &QPushButton::clicked, this, &LibraryScreen::loginRequested);
    v->addWidget(m_login);
    m_pager = new Pager(4, 2, this);
    m_pager->setContentsMargins(Theme::mm(2), Theme::mm(1.5), Theme::mm(2), 0);
    m_pager->setFactory([this](int i) {
        const auto range = PageMath::slice(m_pager->page(), m_works.size(), 8);
        if (i == range.first)
            m_ctx->requestStatus(m_works.mid(range.first, range.second - range.first));
        return m_ctx->makeTile(m_works[i], nullptr, QString(), true);
    });
    v->addWidget(m_pager, 1);
    connect(m_ctx, &AppContext::libraryChanged, this, &LibraryScreen::rebuild);
    rebuild();
}

void LibraryScreen::rebuild()
{
    for (QPushButton *b : m_filters)
        setDark(b, b->property("shelf").toString() == m_shelf);
    setDark(m_downloadedToggle, m_onlyDownloaded);
    const bool guest = !m_ctx->auth->isLoggedIn();
    m_login->setVisible(guest);
    m_works.clear();
    for (const WorkMeta &w : m_ctx->library->works()) {
        if (!m_shelf.isEmpty() && w.libraryState != m_shelf)
            continue;
        if (m_onlyDownloaded && (!m_ctx->index->contains(w.id) || m_ctx->index->fileMissing(w.id)))
            continue;
        m_works << w;
    }
    m_pager->setEmptyText(guest ? QStringLiteral("Войдите, чтобы видеть свою библиотеку.")
                                : QStringLiteral("На этой полке нет книг."));
    m_pager->setCount(m_works.size());
}

// ---------- DownloadsScreen ----------

DownloadsScreen::DownloadsScreen(AppContext *ctx, BookActions *actions, QWidget *parent)
    : Screen(parent)
    , m_ctx(ctx)
    , m_actions(actions)
{
    auto *v = new QVBoxLayout(this);
    v->setContentsMargins(0, 0, 0, 0);
    auto *top = new TopBar(QStringLiteral("Скачанные"), false, this);
    m_ctx->addHeaderButtons(top);
    v->addWidget(top);
    auto *box = new QFrame(this);
    box->setProperty("frame", "box");
    auto *bh = new QHBoxLayout(box);
    m_summary = wrapped(QString(), Theme::smallFont(), box);
    m_updateAll = chip(QStringLiteral("Обновить все"), box);
    m_updateAll->setIcon(Theme::icon(QStringLiteral("rotate")));
    connect(m_updateAll, &QPushButton::clicked, this, [this] {
        // The same books that carry an update badge; details are fetched again before each download.
        const QList<qint64> ids = withUpdates();
        if (ids.isEmpty()) {
            Ui::toast(this, QStringLiteral("Обновлений нет. Проверить их можно кнопкой синхронизации в шапке."));
            return;
        }
        m_actions->updateAll(ids, this);
    });
    bh->addWidget(m_summary, 1);
    bh->addWidget(m_updateAll);
    auto *boxWrap = new QWidget(this);
    auto *bw = new QVBoxLayout(boxWrap);
    bw->setContentsMargins(Theme::mm(2), Theme::mm(1.5), Theme::mm(2), 0);
    bw->addWidget(box);
    v->addWidget(boxWrap);
    m_pager = new Pager(4, 2, this);
    m_pager->setContentsMargins(Theme::mm(2), Theme::mm(1.5), Theme::mm(2), 0);
    m_pager->setEmptyText(QStringLiteral("Скачанных книг пока нет."));
    m_pager->setFactory([this](int i) { return row(i); });
    v->addWidget(m_pager, 1);
    // Tiles update themselves as statuses arrive; only the summary/order is refreshed here,
    // at most every few seconds and only while the screen is visible (rebuilding per status stalled the reader).
    auto *refresh = new QTimer(this);
    refresh->setSingleShot(true);
    refresh->setInterval(3000);
    connect(refresh, &QTimer::timeout, this, [this] {
        if (isVisible())
            rebuild();
    });
    connect(m_ctx, &AppContext::statusChanged, this, [this, refresh](qint64 id) {
        if (m_ids.contains(id) && isVisible() && !refresh->isActive())
            refresh->start();
    });
    rebuild();
}

QList<qint64> DownloadsScreen::withUpdates() const
{
    QList<qint64> ids;
    for (qint64 id : m_ids)
        if (!m_ctx->badgeFor(metaFor(id)).isEmpty())
            ids << id;
    return ids;
}

WorkMeta DownloadsScreen::metaFor(qint64 id) const
{
    if (m_ctx->status->has(id))
        return m_ctx->status->get(id).meta;
    for (const WorkMeta &w : m_ctx->library->works())
        if (w.id == id)
            return w;
    WorkMeta w;
    w.id = id;
    w.title = QFileInfo(m_ctx->index->get(id).path).completeBaseName();
    return w;
}

void DownloadsScreen::rebuild()
{
    m_ids.clear();
    for (const DownloadEntry &e : m_ctx->index->all())
        m_ids << e.workId;
    // Updates first, then most recently downloaded.
    std::sort(m_ids.begin(), m_ids.end(), [this](qint64 a, qint64 b) {
        const bool ua = m_ctx->siteNewer(metaFor(a)), ub = m_ctx->siteNewer(metaFor(b));
        if (ua != ub)
            return ua;
        return m_ctx->index->get(a).downloadedAt > m_ctx->index->get(b).downloadedAt;
    });
    const int updates = withUpdates().size();
    // The reader's library only picks new files up on a rescan.
    m_summary->setText(m_ids.isEmpty() ? QStringLiteral("На читалке пока нет книг из author.today.")
                                       : QStringLiteral("%1 книг на читалке · обновления: %2\nНовые книги появляются в библиотеке читалки "
                                                        "после пересканирования (например, после подключения к компьютеру).")
                                             .arg(m_ids.size()).arg(updates));
    setDark(m_updateAll, updates > 0); // stays tappable: it says why when there is nothing to do
    m_pager->setCount(m_ids.size());
}

QWidget *DownloadsScreen::row(int index)
{
    // Same tile as everywhere; the badge says what is special about the file.
    const qint64 id = m_ids[index];
    const WorkMeta w = metaFor(id);
    QString badge;
    if (m_ctx->index->fileMissing(id))
        badge = QStringLiteral("файл удалён");
    else if (m_ctx->status->has(id) && !m_ctx->status->get(id).allowDownloads)
        badge = QStringLiteral("скачивание закрыто");
    else
        badge = m_ctx->badgeFor(w); // "+N гл." / "обновлено"
    if (badge.isEmpty())
        return m_ctx->makeTile(w, nullptr, QString(), true);
    return m_ctx->makeTile(w, nullptr, badge, true);
}

// ---------- SettingsScreen ----------

namespace {

QWidget *section(const QString &title)
{
    auto *l = new QLabel(title.toUpper());
    QFont f = Theme::smallFont();
    f.setBold(true);
    l->setFont(f);
    l->setContentsMargins(0, Theme::mm(1.5), 0, 0);
    return l;
}

// Square checkbox drawn for e-ink: outline when off, solid black with a white tick when on.
QIcon checkIcon(bool on)
{
    const int s = 64, border = 6;
    QPixmap pm(s, s);
    pm.fill(Qt::transparent);
    QPainter p(&pm);
    p.setRenderHint(QPainter::Antialiasing);
    p.setPen(QPen(Qt::black, border));
    p.setBrush(on ? Qt::black : Qt::white);
    p.drawRoundedRect(QRectF(border / 2.0, border / 2.0, s - border, s - border), 8, 8);
    if (on)
        p.drawPixmap(14, 14, Theme::icon(QStringLiteral("check"), Qt::white).pixmap(36, 36));
    p.end();
    return QIcon(pm);
}

// A whole row is the tap target: checkbox, then the label; thin rule below.
QPushButton *checkRow(const QString &label, bool initial, std::function<void(bool)> changed)
{
    auto *b = new QPushButton(checkIcon(initial), label);
    b->setCheckable(true);
    b->setChecked(initial);
    b->setFont(Theme::smallFont());
    b->setIconSize(QSize(Theme::mm(4), Theme::mm(4)));
    b->setFocusPolicy(Qt::NoFocus);
    b->setStyleSheet(QStringLiteral("QPushButton { border: none; border-bottom: 1px solid #bbb; border-radius: 0;"
                                    " text-align: left; padding: 0; min-height: %1px; }")
                         .arg(Theme::mm(7)));
    QObject::connect(b, &QPushButton::toggled, b, [b, changed](bool on) {
        b->setIcon(checkIcon(on));
        changed(on);
    });
    return b;
}

QWidget *labelRow(const QString &label, QWidget *right)
{
    // Same thin rule below as the checkbox rows.
    auto *w = new QFrame;
    w->setObjectName(QStringLiteral("labelRow"));
    w->setStyleSheet(QStringLiteral("#labelRow { border: none; border-bottom: 1px solid #bbb; }"));
    auto *h = new QHBoxLayout(w);
    h->setContentsMargins(0, Theme::mm(0.8), 0, Theme::mm(0.8));
    auto *l = new QLabel(label, w);
    l->setFont(Theme::smallFont());
    h->addWidget(l, 1);
    right->setParent(w);
    h->addWidget(right);
    w->setMinimumHeight(Theme::mm(7));
    return w;
}

// "EPUB | FB2": one segment per value, the current one inverted.
QWidget *segmented(const QStringList &labels, int current, std::function<void(int)> changed)
{
    auto *w = new QWidget;
    auto *h = new QHBoxLayout(w);
    h->setContentsMargins(0, 0, 0, 0);
    h->setSpacing(0);
    QList<QPushButton *> parts;
    for (int i = 0; i < labels.size(); ++i) {
        auto *b = new QPushButton(labels[i], w);
        b->setFont(Theme::smallFont());
        b->setFocusPolicy(Qt::NoFocus);
        // The global button style has min-height mm(8): without lifting it the text is cut at the bottom.
        b->setStyleSheet(QStringLiteral("QPushButton { min-height: 0; padding: 0; }"));
        b->setFixedSize(Theme::mm(14), Theme::mm(7));
        parts << b;
        h->addWidget(b);
    }
    auto paint = [parts](int sel) {
        for (int i = 0; i < parts.size(); ++i)
            setDark(parts[i], i == sel);
    };
    paint(current);
    for (int i = 0; i < parts.size(); ++i)
        QObject::connect(parts[i], &QPushButton::clicked, w, [paint, changed, i] {
            paint(i);
            changed(i);
        });
    return w;
}

QPushButton *smallButton(const QString &icon, const QString &text, std::function<void()> clicked)
{
    auto *b = new QPushButton(Theme::icon(icon), text);
    b->setFont(Theme::smallFont());
    b->setIconSize(QSize(Theme::mm(3.5), Theme::mm(3.5)));
    b->setFocusPolicy(Qt::NoFocus);
    QObject::connect(b, &QPushButton::clicked, b, [clicked] { clicked(); });
    return b;
}

} // namespace

SettingsScreen::SettingsScreen(AppContext *ctx, QWidget *parent)
    : Screen(parent)
    , m_ctx(ctx)
{
    auto *v = new QVBoxLayout(this);
    v->setContentsMargins(0, 0, 0, 0);
    auto *top = new TopBar(QStringLiteral("Настройки"), true, this);
    ctx->addHeaderButtons(top, false);
    connect(top, &TopBar::back, this, &SettingsScreen::back);
    v->addWidget(top);
    m_body = new QWidget(this);
    v->addWidget(m_body, 1);
    rebuild();
}

void SettingsScreen::rebuild()
{
    // One page (layout C of settings-variants.html). Rebuilt on show: account and menu-icon state change.
    for (QWidget *child : m_body->findChildren<QWidget *>(QString(), Qt::FindDirectChildrenOnly)) {
        child->hide();
        child->deleteLater();
    }
    delete m_body->layout();
    auto *v = new QVBoxLayout(m_body);
    v->setContentsMargins(Theme::mm(3), Theme::mm(1), Theme::mm(3), Theme::mm(2));
    v->setSpacing(Theme::mm(0.8));
    Settings *s = m_ctx->settings;
    auto save = [s] { s->save(); };

    // Account
    {
        const bool in = m_ctx->auth->isLoggedIn();
        auto *who = new QLabel(in ? m_ctx->auth->session().userName : QStringLiteral("Гость"));
        QFont f = Theme::bodyFont();
        f.setBold(true);
        who->setFont(f);
        auto *row = new QWidget;
        auto *h = new QHBoxLayout(row);
        h->setContentsMargins(0, 0, 0, 0);
        auto *icon = new QLabel(row);
        icon->setPixmap(Theme::icon(QStringLiteral("user")).pixmap(Theme::mm(4), Theme::mm(4)));
        h->addWidget(icon);
        who->setParent(row);
        h->addWidget(who, 1);
        QPushButton *b = in ? smallButton(QStringLiteral("right-to-bracket"), QStringLiteral("Выйти"), [this] { emit logout(); })
                            : smallButton(QStringLiteral("right-to-bracket"), QStringLiteral("Войти"), [this] { emit login(); });
        b->setParent(row);
        h->addWidget(b);
        v->addWidget(row);
    }

    v->addWidget(section(QStringLiteral("Подборки на главной")));
    {
        auto *grid = new QWidget;
        auto *g = new QGridLayout(grid);
        g->setContentsMargins(0, 0, 0, 0);
        g->setHorizontalSpacing(Theme::mm(3));
        g->setVerticalSpacing(0);
        int i = 0;
        for (const auto &shelf : allShelves()) {
            const QString key = shelf.first;
            // "Сейчас добавляют в библиотеки" does not fit half a row.
            const QString title = key == QLatin1String("addedToLibraryWorks") ? QStringLiteral("Сейчас добавляют") : shelf.second;
            QPushButton *b = checkRow(title, s->homeShelves.contains(key), [s, key, save](bool on) {
                if (on && !s->homeShelves.contains(key))
                    s->homeShelves << key;
                else if (!on)
                    s->homeShelves.removeAll(key);
                save();
            });
            b->setParent(grid);
            g->addWidget(b, i / 2, i % 2);
            ++i;
        }
        v->addWidget(grid);
    }

    v->addWidget(section(QStringLiteral("Скачивание")));
    v->addWidget(labelRow(QStringLiteral("Формат файла"),
                          segmented({QStringLiteral("EPUB"), QStringLiteral("FB2")}, s->downloadFormat == QLatin1String("Epub") ? 0 : 1,
                                    [s, save](int i) {
                                        s->downloadFormat = i == 0 ? QStringLiteral("Epub") : QStringLiteral("Fb2");
                                        save();
                                    })));
    if (QFileInfo(QStringLiteral("/mnt/ext2")).isDir()) {
        v->addWidget(checkRow(QStringLiteral("Сохранять на карту microSD"), s->storageRoot == QLatin1String("/mnt/ext2"), [this, s, save](bool on) {
            s->storageRoot = on ? QStringLiteral("/mnt/ext2") : QStringLiteral("/mnt/ext1");
            m_ctx->booksDir = Paths::booksDir(s->storageRoot);
            save();
        }));
    }
    v->addWidget(checkRow(QStringLiteral("Открывать книгу после скачивания"), s->openAfterDownload, [s, save](bool on) {
        s->openAfterDownload = on;
        save();
    }));
    v->addWidget(checkRow(QStringLiteral("Отправлять место чтения на сайт"), s->sendPositionToSite, [s, save](bool on) {
        s->sendPositionToSite = on;
        save();
    }));
    v->addWidget(checkRow(QStringLiteral("Предлагать место с сайта при «Читать»"), s->openAtSiteChapter, [s, save](bool on) {
        s->openAtSiteChapter = on;
        save();
    }));
    v->addWidget(checkRow(QStringLiteral("Проверять обновления книг при синхронизации"), s->checkUpdates, [s, save](bool on) {
        s->checkUpdates = on;
        save();
    }));

    v->addWidget(section(QStringLiteral("Каталог и подборки")));
    v->addWidget(checkRow(QStringLiteral("Только книги, которые можно скачать"), s->onlyDownloadable, [s, save](bool on) {
        s->onlyDownloadable = on;
        save();
    }));
    v->addWidget(checkRow(QStringLiteral("Скрывать книги 18+"), s->hideAdult, [s, save](bool on) {
        s->hideAdult = on;
        save();
    }));
    v->addWidget(checkRow(QStringLiteral("Скрывать книги, написанные с помощью ИИ"), s->hideAi, [s, save](bool on) {
        s->hideAi = on;
        save();
    }));
    v->addWidget(checkRow(QStringLiteral("Загружать обложки"), s->loadCovers, [s, save](bool on) {
        s->loadCovers = on;
        save();
    }));

    v->addWidget(section(QStringLiteral("Обслуживание")));
    {
        auto *grid = new QWidget;
        auto *g = new QGridLayout(grid);
        g->setContentsMargins(0, Theme::mm(1), 0, 0);
        g->setSpacing(Theme::mm(1.5));
        const QString view = DesktopIcon::viewJsonPath();
        const bool iconInstalled = DesktopIcon::isInstalled(view);
        QList<QPushButton *> buttons{
            smallButton(QStringLiteral("xmark"), QStringLiteral("Очистить кэш"), [this] {
                m_ctx->covers->clear();
                m_ctx->status->clear();
                Ui::toast(this, QStringLiteral("Кэш очищен (обложки и статусы книг)"));
            }),
            // Opt-in: edits the firmware's menu file (with a backup), so it asks first.
            smallButton(QStringLiteral("tablet-screen-button"),
                        iconInstalled ? QStringLiteral("Убрать иконку") : QStringLiteral("Иконка в меню"), [this, view] {
                const bool installed = DesktopIcon::isInstalled(view);
                const QString warn = installed
                    ? QStringLiteral("Убрать запись InkToday из файла меню приложений (system/config/desktop/view.json)?")
                    : QStringLiteral("Приложение допишет свою запись в файл меню приложений прошивки "
                                     "(system/config/desktop/view.json). Перед первым изменением файл копируется в "
                                     "view.json.inktoday-backup. Если меню сломается, верните копию по USB. Продолжить?");
                if (!Ui::confirm(this, warn, installed ? QStringLiteral("Убрать") : QStringLiteral("Добавить")))
                    return;
                Ui::toast(this, DesktopIcon::message(installed ? DesktopIcon::remove(view) : DesktopIcon::install(view)));
                QTimer::singleShot(0, this, &SettingsScreen::rebuild);
            }),
            smallButton(QStringLiteral("file-lines"), QStringLiteral("Журнал"), [this] { emit openLog(); }),
            smallButton(QStringLiteral("circle-info"), QStringLiteral("О программе"), [this] { emit openAbout(); }),
        };
        for (int i = 0; i < buttons.size(); ++i) {
            buttons[i]->setParent(grid);
            g->addWidget(buttons[i], i / 2, i % 2);
        }
        v->addWidget(grid);
    }
    v->addStretch(1);
}

// ---------- AboutScreen ----------

AboutScreen::AboutScreen(QWidget *parent)
    : Screen(parent)
{
    auto *v = new QVBoxLayout(this);
    v->setContentsMargins(0, 0, 0, 0);
    auto *top = new TopBar(QStringLiteral("О программе"), true, this);
    connect(top, &TopBar::back, this, &AboutScreen::back);
    v->addWidget(top);

    m_blocks << Block{Block::Logo, {}}
             << Block{Block::Text, QStringLiteral("Версия %1").arg(QStringLiteral(APP_VERSION))}
             << Block{Block::Text, QStringLiteral("Неофициальный клиент author.today для поиска, синхронизации и чтения книг. "
                                                  "Не связан с порталом author.today. Книги и оплата — на сайте.")}
             << Block{Block::Text, QStringLiteral("Книги, у которых автор закрыл скачивание, приложение не скачивает и не сохраняет.")};
    // LICENSES.ru.md: "#" lines are headings; a paragraph's hard line breaks are joined (the label
    // wraps by itself), only "- " list items keep their own lines.
    QFile f(QStringLiteral(":/LICENSES.ru.md"));
    if (f.open(QIODevice::ReadOnly)) {
        for (const QString &para : QString::fromUtf8(f.readAll()).split(QStringLiteral("\n\n"), QString::SkipEmptyParts)) {
            const QString t = para.trimmed();
            if (t.startsWith(QLatin1Char('#'))) {
                m_blocks << Block{Block::Heading, QString(t).remove(QLatin1Char('#')).trimmed()};
                continue;
            }
            QStringList lines;
            for (const QString &line : t.split(QLatin1Char('\n'))) {
                if (!lines.isEmpty() && !line.startsWith(QLatin1String("- ")))
                    lines.last() += QLatin1Char(' ') + line.trimmed();
                else
                    lines << line.trimmed();
            }
            m_blocks << Block{Block::Text, lines.join(QLatin1Char('\n')).remove(QLatin1Char('`'))};
        }
    }
    m_pager = new Pager(1, 1, this);
    m_pager->setContentsMargins(Theme::mm(4), Theme::mm(3), Theme::mm(4), 0);
    m_pager->setFactory([this](int i) {
        auto *column = new QWidget;
        auto *cv = new QVBoxLayout(column);
        cv->setContentsMargins(0, 0, 0, 0);
        cv->setSpacing(Theme::mm(1.5));
        for (int b = m_pages[i].first; b < m_pages[i].second; ++b)
            cv->addWidget(blockWidget(m_blocks[b]));
        cv->addStretch(1);
        return column;
    });
    v->addWidget(m_pager, 1);
}

QWidget *AboutScreen::blockWidget(const Block &b) const
{
    if (b.kind == Block::Logo)
        return Ui::logo(Theme::mm(30), nullptr);
    QFont font = Theme::smallFont();
    if (b.kind == Block::Heading)
        font.setWeight(QFont::Bold);
    auto *l = wrapped(b.text, font, nullptr);
    l->setAlignment(Qt::AlignTop | Qt::AlignLeft);
    if (b.kind == Block::Heading)
        l->setContentsMargins(0, Theme::mm(2), 0, 0);
    return l;
}

void AboutScreen::resizeEvent(QResizeEvent *event)
{
    Screen::resizeEvent(event);
    if (m_pager->size() != m_laidOutFor)
        paginate();
}

void AboutScreen::paginate()
{
    // Pack whole paragraphs onto pages by their wrapped height at the real page width.
    m_laidOutFor = m_pager->size();
    const QRect area = m_pager->contentsRect();
    const int width = area.width();
    const int footer = QFontMetrics(Theme::smallFont()).height() + Theme::mm(1);
    const int height = area.height() - footer;
    if (width <= 0 || height <= 0)
        return;
    m_pages.clear();
    int from = 0, used = 0;
    for (int i = 0; i < m_blocks.size(); ++i) {
        QWidget *w = blockWidget(m_blocks[i]);
        const int h = (w->hasHeightForWidth() ? w->heightForWidth(width) : w->sizeHint().height()) + Theme::mm(1.5);
        delete w;
        if (used + h > height && i > from) {
            m_pages << qMakePair(from, i);
            from = i;
            used = 0;
        }
        used += h;
    }
    m_pages << qMakePair(from, m_blocks.size());
    m_pager->setCount(m_pages.size());
}
