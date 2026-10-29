#include "AppController.h"

#include "ApiClient.h"
#include "AppContext.h"
#include "AuthService.h"
#include "BookActions.h"
#include "BookScreens.h"
#include "MyScreens.h"
#include "CoverCache.h"
#include "Device.h"
#include "DownloadIndex.h"
#include "Downloader.h"
#include "HomeFeed.h"
#include "CatalogScreens.h"
#include "HomeScreens.h"
#include "AuthorScreens.h"
#include "LibraryActions.h"
#include "LibraryStore.h"
#include "LibrarySync.h"
#include "Log.h"
#include "Paths.h"
#include "QtHttpTransport.h"
#include "PositionUpload.h"
#include "ReaderLibrary.h"
#include "ReadingPosition.h"
#include "Session.h"
#include "Settings.h"
#include "StartScreen.h"
#include "StartupFlow.h"
#include "StatusCache.h"
#include "Theme.h"
#include "UpdateCheck.h"
#include "Widgets.h"

#include <QApplication>
#include <QDateTime>
#include <QDialog>
#include <QDir>
#include <QFontDatabase>
#include <QFile>
#include <QGuiApplication>
#include <QAbstractButton>
#include <QKeyEvent>
#include <QMouseEvent>
#include <QPointer>
#include <QResizeEvent>
#include <QScreen>
#include <QStackedWidget>
#include <QTimer>
#include <QVBoxLayout>

namespace {

// The mount holding `path` and its file system, e.g. "/var/tmp tmpfs" (from /proc/mounts).
QString mountOf(const QString &path)
{
    QFile f(QStringLiteral("/proc/mounts"));
    if (!f.open(QIODevice::ReadOnly))
        return QStringLiteral("unknown");
    QString best, type;
    for (const QByteArray &line : f.readAll().split('\n')) {
        const QList<QByteArray> parts = line.split(' ');
        if (parts.size() < 3)
            continue;
        const QString point = QString::fromUtf8(parts[1]);
        const bool inside = path == point || path.startsWith(point.endsWith(QLatin1Char('/')) ? point : point + QLatin1Char('/'));
        if (inside && point.size() > best.size()) {
            best = point;
            type = QString::fromUtf8(parts[2]);
        }
    }
    return best.isEmpty() ? QStringLiteral("unknown") : best + QLatin1Char(' ') + type;
}

} // namespace

AppController::AppController(QWidget *parent)
    : QWidget(parent)
{
    setStyleSheet(Theme::styleSheet());
    // Body text and button labels: buttons use the smaller size (labels at body size did not fit).
    QApplication::setFont(Theme::bodyFont());
    QApplication::setFont(Theme::smallFont(), "QPushButton");
    buildServices();

    // The firmware draws its status panel (clock, battery) above us and the window ended below
    // the screen edge: keep the content inside the visible area.
    const QScreen *screen = QGuiApplication::primaryScreen();
    const QRect sg = screen ? screen->geometry() : QRect(0, 0, 825, 1200);
    const int panel = device::panelHeight();
    Log::write(QStringLiteral("screen %1x%2, Qt dpi %3 (physical %4), firmware dpi %5, panel %6, mm(8)=%7px")
                   .arg(sg.width()).arg(sg.height())
                   .arg(screen ? screen->logicalDotsPerInch() : 0)
                   .arg(screen ? screen->physicalDotsPerInch() : 0)
                   .arg(device::screenDpi()).arg(panel).arg(Theme::mm(8)));
    // Fixed height too: otherwise the tallest page anywhere in the stacks stretched the window for
    // every page, and the tab bar slid below the screen.
    setFixedSize(sg.width(), sg.height());
    Log::write(QStringLiteral("font family: '%1' of %2").arg(Theme::fontFamily(), QFontDatabase().families().join(QStringLiteral(", "))));

    auto *v = new QVBoxLayout(this);
    v->setContentsMargins(0, 0, 0, qMax(0, panel));
    m_root = new QStackedWidget(this);
    m_root->setSizePolicy(QSizePolicy::Preferred, QSizePolicy::Ignored);
    v->addWidget(m_root);

    m_start = new StartScreen(m_flow.get(), m_auth.get(), m_root);
    m_start->setNetworkGate(m_ctx->networkGate());
    m_root->addWidget(m_start);
    connect(m_start, &StartScreen::finished, this, [this] {
        if (m_main) {
            // Logged in (or gave up) from inside the app: back to where the user was, refreshed
            // (settings show the account, the card its buttons).
            m_root->setCurrentWidget(m_main);
            refreshCurrentScreen();
            if (m_auth->isLoggedIn())
                syncAll(true);
            return;
        }
        showMain(0);
        syncAll(true);
        m_ctx->reloadGenres(); // the first load ran before Wi-Fi was up
        m_ctx->updates->run(); // once per start, only when online
    });
    connect(m_client.get(), &ApiClient::unauthorized, m_auth.get(), &AuthService::recover);
    connect(m_auth.get(), &AuthService::sessionLost, this, [this] {
        Ui::toast(this, QStringLiteral("Вход больше не действует, войдите снова"));
        emit m_ctx->libraryChanged(); // buttons follow the guest state
    });
    connect(m_start, &StartScreen::offline, this, [this] { showMain(3); });

    qApp->installEventFilter(this);
    device::installKeyHandler([this](int key) { handleKey(key); });
    device::installForegroundHandler([this] { checkReaderLibrary(); });
}

AppController::~AppController() = default;

void AppController::buildServices()
{
    const QString data = Paths::dataDir();
    auto nowMs = [] { return QDateTime::currentMSecsSinceEpoch(); };
    auto now = [] { return QDateTime::currentDateTimeUtc(); };

    m_transport.reset(new QtHttpTransport(QCoreApplication::applicationDirPath() + QStringLiteral("/cacert.pem")));
    m_client.reset(new ApiClient(m_transport.get(), nowMs));
    m_sessions.reset(new SessionStore(data + QStringLiteral("/session.json")));
    m_auth.reset(new AuthService(m_client.get(), m_sessions.get(), now));
    m_library.reset(new LibraryStore(data + QStringLiteral("/library.json")));
    m_sync.reset(new LibrarySync(m_client.get(), m_library.get(), now));
    m_index.reset(new DownloadIndex(data + QStringLiteral("/downloads.json")));
    m_downloader.reset(new Downloader(m_client.get(), m_index.get()));
    m_settings.reset(new Settings(data + QStringLiteral("/settings.json")));
    m_feed.reset(new HomeFeed(m_client.get(), m_settings.get(), data + QStringLiteral("/home.json")));
    m_status.reset(new StatusCache(m_client.get(), data + QStringLiteral("/status.json"), now));
    m_covers.reset(new CoverCache(m_client.get(), data + QStringLiteral("/covers")));
    m_libraryActions.reset(new LibraryActions(m_client.get(), m_library.get()));

    // Another account's books must never show up after logout or an account switch.
    // A sync still in flight would bring the old account's books back: cancel it first.
    auto forgetAccount = [this] {
        m_sync->cancel();
        m_syncing = false;
        m_library->clear();
        m_status->clear(); // purchase / download rights were the old account's
    };
    connect(m_auth.get(), &AuthService::loggedOut, this, forgetAccount);
    connect(m_auth.get(), &AuthService::userChanged, this, forgetAccount);

    m_ctx = new AppContext;
    m_status->setNetworkGate(m_ctx->networkGate());
    m_ctx->setParent(this);
    m_ctx->client = m_client.get();
    m_ctx->auth = m_auth.get();
    m_ctx->library = m_library.get();
    m_ctx->sync = m_sync.get();
    m_ctx->index = m_index.get();
    m_ctx->downloader = m_downloader.get();
    m_ctx->settings = m_settings.get();
    m_ctx->feed = m_feed.get();
    m_ctx->status = m_status.get();
    m_ctx->covers = m_covers.get();
    m_ctx->libraryActions = m_libraryActions.get();
    m_ctx->booksDir = Paths::booksDir(m_settings->storageRoot);
    m_ctx->updates = new UpdateCheck(m_transport.get(), QStringLiteral(APP_VERSION), m_ctx);
    // Positions in the reader's own library; the copy goes to the system temp folder (RAM on the device).
    m_readerLibrary.reset(new ReaderLibrary(QStringLiteral("/mnt/ext1/system/explorer-3/explorer-3.db"),
                                            QDir::tempPath() + QStringLiteral("/inktoday-reader")));
    m_ctx->readerLibrary = m_readerLibrary.get();
    m_uploader.reset(new PositionUploader(m_client.get(), data + QStringLiteral("/sent-positions.json")));
    m_ctx->uploader = m_uploader.get();
    Log::write(QStringLiteral("temp folder for the reader library copy: %1 (%2)").arg(QDir::tempPath(), mountOf(QDir::tempPath())));
    connect(m_status.get(), &StatusCache::updated, m_ctx, &AppContext::emitStatusChanged);
    connect(m_ctx, &AppContext::openBook, this, &AppController::openBook);
    connect(m_ctx, &AppContext::action, this, &AppController::runAction);
    connect(m_ctx, &AppContext::syncRequested, this, [this] { syncAll(false); });
    connect(m_ctx, &AppContext::openSettings, this, &AppController::openSettings);
    // Search and an author's books open on top of whatever tab asked for them.
    connect(m_ctx, &AppContext::openAuthor, this, [this](const QString &login, const QString &name) {
        Navigator *nav = currentNavigator();
        auto *author = new AuthorWorksScreen(m_ctx, login, name);
        connect(author, &AuthorWorksScreen::back, nav, &Navigator::pop);
        pushScreen(author);
    });
    m_actions = new BookActions(m_ctx, this);
    connect(m_actions, &BookActions::openCard, this, &AppController::openBook);
    connect(m_actions, &BookActions::loginRequested, this, [this] {
        m_start->requestLogin();
        m_root->setCurrentWidget(m_start);
    });
    m_ctx->loadGenres(data + QStringLiteral("/genres.json"));

    StartupFlow::Deps deps;
    deps.networkConnected = [] { return device::networkConnected(); };
    deps.connectNetwork = m_ctx->networkGate();
    deps.pingServer = [this](std::function<void(bool)> done) {
        m_client->get(QStringLiteral("/v1/monitoring/heartbeat"),
                      [done](const QJsonDocument &, const ApiError *e) { done(!e || !e->network); });
    };
    deps.hasSession = [this] { return m_auth->hasToken(); };
    deps.refreshSession = [this](std::function<void(bool)> done) { m_auth->refreshIfNeeded(std::move(done)); };
    m_flow.reset(new StartupFlow(deps));
}

void AppController::checkReaderLibrary()
{
    if (!m_main || !m_readerLibrary)
        return;
    const QString stamp = m_readerLibrary->currentStamp();
    if (stamp == m_readerStamp)
        return;
    const bool first = m_readerStamp.isEmpty();
    m_readerStamp = stamp;
    if (!first)
        QTimer::singleShot(0, this, [this] { readerLibraryChanged(); });
}

void AppController::readerLibraryChanged()
{
    Log::write(QStringLiteral("reader library changed: refreshing the screen"));
    refreshCurrentScreen();
    if (!m_settings->sendPositionToSite || !m_auth->isLoggedIn() || m_syncing)
        return;
    QPointer<AppController> self(this);
    m_ctx->networkGate()([self](bool online) {
        if (self && online)
            self->uploadPositions();
    });
}

void AppController::refreshCurrentScreen()
{
    Log::debug(QStringLiteral("refresh the current screen"));
    if (Screen *s = currentNavigator() ? currentNavigator()->current() : nullptr)
        s->shown();
}

void AppController::start()
{
    Log::write(QStringLiteral("window geometry %1,%2 %3x%4")
                   .arg(geometry().x()).arg(geometry().y()).arg(geometry().width()).arg(geometry().height()));
    m_root->setCurrentWidget(m_start);
    m_flow->start();
}

void AppController::showMain(int tab)
{
    if (!m_main) {
        m_main = new QWidget(m_root);
        auto *v = new QVBoxLayout(m_main);
        v->setContentsMargins(0, 0, 0, 0);
        v->setSpacing(0);
        m_tabs = new QStackedWidget(m_main);
        m_tabs->setSizePolicy(QSizePolicy::Preferred, QSizePolicy::Ignored); // a tall page is cut, the tab bar stays
        m_tabBar = new TabBar(m_main);
        v->addWidget(m_tabs, 1);
        v->addWidget(m_tabBar);

        auto *homeNav = new Navigator(m_tabs);
        m_home = new HomeScreen(m_ctx);
        connect(m_home, &HomeScreen::openShelf, this, [this, homeNav](const QString &title, const QList<WorkMeta> &works) {
            auto *shelf = new ShelfScreen(m_ctx, title, works);
            connect(shelf, &ShelfScreen::back, homeNav, &Navigator::pop);
            homeNav->push(shelf);
        });
        connect(m_home, &HomeScreen::openLibraryTab, this, [this] { selectTab(2); });
        homeNav->setRoot(m_home);
        m_tabs->addWidget(homeNav);

        auto *catalogNav = new Navigator(m_tabs);
        auto *catalog = new CatalogScreen(m_ctx);
        connect(catalog, &CatalogScreen::pickGenre, this, [this, catalog, catalogNav](const QString &current) {
            m_ctx->reloadGenres(); // empty: try again for the next visit
            auto *genres = new GenreScreen(m_ctx->genreList, current);
            connect(genres, &GenreScreen::back, catalogNav, &Navigator::pop);
            connect(genres, &GenreScreen::chosen, this, [catalog, catalogNav](const QString &code, const QString &title) {
                catalogNav->pop();
                catalog->setGenre(code, title);
            });
            catalogNav->push(genres);
        });
        catalogNav->setRoot(catalog);
        // The magnifier on Home: the search field lives in the catalog.
        connect(m_ctx, &AppContext::openSearch, this, [this, catalog, catalogNav] {
            while (catalogNav->depth() > 1)
                catalogNav->pop();
            selectTab(1);
            catalog->focusSearch();
        });
        connect(m_ctx, &AppContext::openTag, this, [this, catalog, catalogNav](const QString &tag) {
            while (catalogNav->depth() > 1)
                catalogNav->pop();
            catalog->setTag(tag);
            selectTab(1);
        });
        m_tabs->addWidget(catalogNav);

        auto *libraryNav = new Navigator(m_tabs);
        auto *library = new LibraryScreen(m_ctx);
        connect(library, &LibraryScreen::loginRequested, this, [this] {
            m_start->requestLogin();
            m_root->setCurrentWidget(m_start);
        });
        libraryNav->setRoot(library);
        m_tabs->addWidget(libraryNav);

        auto *downloadsNav = new Navigator(m_tabs);
        downloadsNav->setRoot(new DownloadsScreen(m_ctx, m_actions));
        m_tabs->addWidget(downloadsNav);

        connect(m_tabBar, &TabBar::tabSelected, this, &AppController::selectTab);
        m_root->addWidget(m_main);
    }
    m_root->setCurrentWidget(m_main);
    selectTab(tab);
}

void AppController::selectTab(int tab)
{
    Log::debug(QStringLiteral("tab %1").arg(tab));
    m_tabs->setCurrentIndex(tab);
    m_tabBar->setCurrent(tab);
    if (Screen *s = currentNavigator() ? currentNavigator()->current() : nullptr)
        s->shown();
    // No forced flash: like the reader, let the panel update what changed (full refreshes are
    // kept for closing dialogs, which leave traces otherwise).
}

Navigator *AppController::currentNavigator() const
{
    return m_tabs ? qobject_cast<Navigator *>(m_tabs->currentWidget()) : nullptr;
}

void AppController::handleKey(int key)
{
    // Each hardware press arrives twice (InkView handler and Qt key event): pages went 1-3-5-7.
    const qint64 now = QDateTime::currentMSecsSinceEpoch();
    if (key == m_lastKey && now - m_lastKeyAt < 300)
        return;
    m_lastKey = key;
    m_lastKeyAt = now;
    if (key == device::KeyBack) {
        // Back on an open dialog closes it (left alone, the firmware sent the app to the background).
        if (auto *dialog = qobject_cast<QDialog *>(QApplication::activeModalWidget())) {
            dialog->reject();
            return;
        }
    }
    if (key == device::KeyBack && m_main && m_root->currentWidget() == m_start && !QApplication::activeModalWidget()) {
        m_start->clearFields();
        m_root->setCurrentWidget(m_main); // the login page opened from inside the app
        return;
    }
    Navigator *nav = currentNavigator();
    if (!nav || m_root->currentWidget() != m_main || QApplication::activeModalWidget())
        return;
    if (key == device::KeyBack)
        nav->pop();
    else if (Screen *s = nav->current())
        s->pageKey(key);
}

bool AppController::eventFilter(QObject *watched, QEvent *event)
{
    // Coming back from the reader: the firmware's foreground event, and as a fallback the first touch
    // or key (the touch still acts on what was on screen; the refresh follows).
    if (event->type() == QEvent::MouseButtonRelease || event->type() == QEvent::KeyPress)
        checkReaderLibrary();
    if (event->type() == QEvent::Resize && qobject_cast<Screen *>(watched))
        Log::debug(QStringLiteral("resize %1 %2x%3").arg(QString::fromLatin1(watched->metaObject()->className()))
                       .arg(static_cast<QResizeEvent *>(event)->size().width()).arg(static_cast<QResizeEvent *>(event)->size().height()));
    switch (event->type()) {
    case QEvent::Polish:
        // No keyboard focus on buttons: the focus frame was drawn smaller than the button.
        if (auto *button = qobject_cast<QAbstractButton *>(watched))
            button->setFocusPolicy(Qt::NoFocus);
        break;
    case QEvent::MouseButtonPress:
        m_pressPos = static_cast<QMouseEvent *>(event)->globalPos();
        break;
    case QEvent::MouseButtonRelease: {
        // Horizontal swipe anywhere = page flip (right-to-left: next page).
        const QPoint d = static_cast<QMouseEvent *>(event)->globalPos() - m_pressPos;
        if (!m_pressPos.isNull() && qAbs(d.x()) > Theme::mm(15) && qAbs(d.x()) > 2 * qAbs(d.y())) {
            m_pressPos = QPoint();
            // The release is swallowed: the button pressed at the start would stay drawn pressed.
            if (auto *b = qobject_cast<QAbstractButton *>(watched))
                b->setDown(false);
            handleKey(d.x() < 0 ? device::KeyNext : device::KeyPrev);
            return true; // not a click
        }
        break;
    }
    default:
        break;
    }
    if (event->type() == QEvent::KeyPress) {
        const int k = static_cast<QKeyEvent *>(event)->key();
        if (k == Qt::Key_PageDown || k == Qt::Key_PageUp) {
            handleKey(k == Qt::Key_PageDown ? device::KeyNext : device::KeyPrev);
            return true;
        }
    }
    return QWidget::eventFilter(watched, event);
}

void AppController::syncAll(bool quiet)
{
    if (m_syncing) {
        if (!quiet)
            Ui::toast(this, QStringLiteral("Синхронизация уже идёт"));
        return;
    }
    // The firmware turns Wi-Fi off on its own; there is no API to keep it on, so reconnect quietly.
    if (!device::networkConnected()) {
        m_syncing = true;
        QPointer<AppController> self(this);
        m_ctx->networkGate()([self, quiet](bool ok) { // answers within its timeout
            if (!self)
                return;
            self->m_syncing = false;
            if (ok)
                self->syncAll(quiet);
            else
                Ui::toast(self, QStringLiteral("Нет подключения к интернету"));
        });
        return;
    }
    m_syncing = true;
    if (!quiet)
        Ui::toast(this, QStringLiteral("Синхронизация…"));
    auto finish = [this, quiet](const QString &error) {
        m_syncing = false;
        emit m_ctx->libraryChanged(); // Home rebuilds on it
        if (!error.isEmpty())
            Ui::toast(this, error);
        else if (!quiet)
            Ui::toast(this, QStringLiteral("Готово"));
        // Re-check downloaded books for updates, unless the user turned it off.
        if (m_settings->checkUpdates) {
            QList<qint64> ids;
            for (const DownloadEntry &e : m_index->all())
                ids << e.workId;
            m_status->request(ids);
        }
        if (error.isEmpty() && m_settings->sendPositionToSite && m_auth->isLoggedIn())
            uploadPositions();
    };
    m_feed->refresh([this, finish](const ApiError *feedError) {
        const QString feedMsg = feedError ? feedError->message : QString();
        m_auth->refreshIfNeeded([this, finish, feedMsg](bool loggedIn) {
            if (!loggedIn) {
                finish(feedMsg);
                return;
            }
            m_sync->sync([finish, feedMsg](const ApiError *e) { finish(e ? e->message : feedMsg); });
        });
    });
}

void AppController::pushScreen(Screen *screen)
{
    Navigator *nav = currentNavigator();
    if (!nav) {
        screen->deleteLater();
        return;
    }
    nav->push(screen);
}

void AppController::openBook(qint64 id)
{
    auto *card = new BookScreen(m_ctx, m_actions, id);
    Navigator *nav = currentNavigator();
    connect(card, &BookScreen::back, nav, &Navigator::pop);
    connect(card, &BookScreen::openSeries, this, [this, nav](const WorkDetails &d) {
        auto *series = new SeriesScreen(m_ctx, d);
        connect(series, &SeriesScreen::back, nav, &Navigator::pop);
        nav->push(series);
    });
    connect(card, &BookScreen::openShelf, this, [this, nav](const QString &title, const QList<WorkMeta> &works) {
        auto *shelf = new ShelfScreen(m_ctx, title, works);
        connect(shelf, &ShelfScreen::back, nav, &Navigator::pop);
        nav->push(shelf);
    });
    connect(card, &BookScreen::openText, this, [nav](const QString &title, const QString &text) {
        auto *t = new TextScreen(title, text);
        connect(t, &TextScreen::back, nav, &Navigator::pop);
        nav->push(t);
    });
    pushScreen(card);
}

void AppController::runAction(qint64 id, ActionState state)
{
    m_actions->run(id, state, this);
}

void AppController::openSettings()
{
    Navigator *nav = currentNavigator();
    if (!nav)
        return;
    auto *settings = new SettingsScreen(m_ctx);
    connect(settings, &SettingsScreen::back, nav, [this, nav] {
        nav->pop();
        if (m_home)
            m_home->rebuild(); // shelves or filters may have changed
    });
    connect(settings, &SettingsScreen::logout, this, [this, nav] {
        if (!Ui::confirm(this, QStringLiteral("Выйти из аккаунта? Скачанные книги останутся на читалке."), QStringLiteral("Выйти")))
            return;
        m_auth->logout();
        nav->pop();
        emit m_ctx->libraryChanged(); // Home rebuilds on it
        Ui::toast(this, QStringLiteral("Вы вышли из аккаунта"));
    });
    connect(settings, &SettingsScreen::login, this, [this] {
        m_start->requestLogin();
        m_root->setCurrentWidget(m_start);
    });
    connect(settings, &SettingsScreen::openAbout, this, [nav] {
        auto *about = new AboutScreen;
        QObject::connect(about, &AboutScreen::back, nav, &Navigator::pop);
        nav->push(about);
    });
    connect(settings, &SettingsScreen::openLog, this, [nav] {
        // Only the end of the file: the whole log can be megabytes.
        auto *log = new TextScreen(QStringLiteral("Журнал"), Log::tail(400).join(QLatin1Char('\n')));
        QObject::connect(log, &TextScreen::back, nav, &Navigator::pop);
        nav->push(log);
    });
    nav->push(settings);
}

void AppController::uploadPositions()
{
    // Downloaded books whose place in the reader changed since we last looked; at most 10 a sync,
    // one at a time (each needs fresh chapters from the site). A book counts as looked at only
    // once the site has its place or the place need not go: a failed try is repeated next time.
    struct Todo {
        qint64 id;
        QString path;
        QString position;
    };
    QList<Todo> todo;
    for (const DownloadEntry &e : m_index->all()) {
        if (todo.size() >= 10)
            break;
        if (m_index->fileMissing(e.workId))
            continue;
        const ReaderPosition p = m_readerLibrary->positionOf(e.path);
        if (!p.found || p.completed || m_checkedPositions.value(e.workId) == p.position)
            continue;
        todo << Todo{e.workId, e.path, p.position};
    }
    auto next = std::make_shared<std::function<void(int)>>();
    std::weak_ptr<std::function<void(int)>> weakNext = next;
    QPointer<AppController> self(this);
    *next = [self, todo, weakNext](int i) {
        if (!self || i >= todo.size())
            return;
        auto step = weakNext.lock();
        const Todo t = todo[i];
        auto done = [self, step, t, i](bool checked) {
            if (self && checked)
                self->m_checkedPositions.insert(t.id, t.position);
            if (step)
                (*step)(i + 1);
        };
        self->m_client->get(QStringLiteral("/v1/work/%1/details").arg(t.id), [self, t, done](const QJsonDocument &doc, const ApiError *e) {
            if (!self)
                return;
            if (e) {
                done(false);
                return;
            }
            const WorkDetails details = WorkDetails::fromJson(doc.object());
            const ReaderPosition p = self->m_readerLibrary->positionOf(t.path);
            const int spine = spineIndexFromPosition(p.position);
            const QList<int> lengths = readEpubTextLengths(t.path);
            const ChapterMap map = mapChapters(readEpubToc(t.path), details.chapters, lengths.size());
            // Only a map confirmed by titles: a guessed one could push the site to an unread chapter.
            const qint64 chapterId = map.exact ? map.chapterBySpine.value(spine) : 0;
            if (!chapterId) {
                done(true);
                return;
            }
            const double percent = groupPercent(map.spinesByChapter.value(chapterId), lengths, spine,
                                                chapterPercentFromPosition(p.position, readEpubSpineFile(t.path, spine)));
            const UploadDecision d = decideUpload(details, chapterId, percent, p.savedAt, self->m_uploader->openedAt(t.id));
            if (!d.send || self->m_uploader->alreadySent(t.id, d)) {
                done(true);
                return;
            }
            self->m_uploader->upload(t.id, d, done);
        });
    };
    (*next)(0);
}
