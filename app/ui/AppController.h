#pragma once

#include "BookStatus.h"
#include "Models.h"

#include <QHash>
#include <QWidget>

#include <memory>

class AppContext;
class BookActions;
class ApiClient;
class AuthService;
class CoverCache;
class DownloadIndex;
class Downloader;
class HomeFeed;
class HomeScreen;
class LibraryActions;
class LibraryStore;
class LibrarySync;
class Navigator;
class QStackedWidget;
class QtHttpTransport;
class ReaderLibrary;
class PositionUploader;
class SessionStore;
class Settings;
class StartScreen;
class StartupFlow;
class StatusCache;
class Screen;
class TabBar;

// Owns the services and the window: start screen first, then four tabs, each with its own screen stack.
class AppController : public QWidget {
    Q_OBJECT
public:
    explicit AppController(QWidget *parent = nullptr);
    ~AppController() override;

    void start();

protected:
    bool eventFilter(QObject *watched, QEvent *event) override;

private:
    void refreshCurrentScreen();
    void checkReaderLibrary();   // the reader saved something since we last looked: readerLibraryChanged()
    void readerLibraryChanged(); // back from the reader: refresh, send the new place to the site
    QString m_readerStamp;       // the reader library's state the screen was last refreshed for
    void buildServices();
    void showMain(int tab);
    void selectTab(int tab);
    Navigator *currentNavigator() const;
    void handleKey(int key);
    void syncAll(bool quiet);
    void uploadPositions(); // reader -> site after a sync, if the setting is on
    void openBook(qint64 id);
    void runAction(qint64 id, ActionState state);
    void openSettings();
    void pushScreen(Screen *screen);

    std::unique_ptr<PositionUploader> m_uploader;
    QHash<qint64, QString> m_checkedPositions; // workId -> reader position already looked at
    std::unique_ptr<ReaderLibrary> m_readerLibrary;
    std::unique_ptr<QtHttpTransport> m_transport;
    std::unique_ptr<ApiClient> m_client;
    std::unique_ptr<SessionStore> m_sessions;
    std::unique_ptr<AuthService> m_auth;
    std::unique_ptr<LibraryStore> m_library;
    std::unique_ptr<LibrarySync> m_sync;
    std::unique_ptr<DownloadIndex> m_index;
    std::unique_ptr<Downloader> m_downloader;
    std::unique_ptr<Settings> m_settings;
    std::unique_ptr<HomeFeed> m_feed;
    std::unique_ptr<StatusCache> m_status;
    std::unique_ptr<CoverCache> m_covers;
    std::unique_ptr<LibraryActions> m_libraryActions;
    std::unique_ptr<StartupFlow> m_flow;
    AppContext *m_ctx = nullptr;
    BookActions *m_actions = nullptr;

    QStackedWidget *m_root = nullptr;  // 0: start screen, 1: main
    StartScreen *m_start = nullptr;
    QWidget *m_main = nullptr;
    QStackedWidget *m_tabs = nullptr;  // one Navigator per tab
    TabBar *m_tabBar = nullptr;
    HomeScreen *m_home = nullptr;
    bool m_syncing = false;
    int m_lastKey = -100;
    QPoint m_pressPos;
    qint64 m_lastKeyAt = 0;
};
