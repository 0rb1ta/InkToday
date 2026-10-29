#pragma once

#include "BookStatus.h"
#include "Models.h"
#include "WorkList.h"

#include <QHash>
#include <QObject>

#include <memory>

class ApiClient;
class AuthService;
class CoverCache;
class DownloadIndex;
class Downloader;
class HomeFeed;
class LibraryActions;
class UpdateCheck;
class ReaderLibrary;
class LibraryStore;
class LibrarySync;
class Settings;
class NetworkGate;
class TopBar;
class PositionUploader;
class StatusCache;
class Tile;
class QWidget;

// Services shared by all screens plus the rules that turn them into what a tile shows.
class AppContext : public QObject {
    Q_OBJECT
public:
    AppContext();
    ~AppContext() override;
    ApiClient *client = nullptr;
    AuthService *auth = nullptr;
    LibraryStore *library = nullptr;
    LibrarySync *sync = nullptr;
    DownloadIndex *index = nullptr;
    Downloader *downloader = nullptr;
    Settings *settings = nullptr;
    HomeFeed *feed = nullptr;
    StatusCache *status = nullptr;
    CoverCache *covers = nullptr;
    LibraryActions *libraryActions = nullptr;
    UpdateCheck *updates = nullptr;
    ReaderLibrary *readerLibrary = nullptr; // the firmware library, read from a copy
    PositionUploader *uploader = nullptr;   // sends the reader's place to the site
    QString booksDir;
    QHash<int, QString> genres;
    QList<Genre> genreList; // the same /v1/work/genres answer as a tree (catalog filter)

    ActionState stateFor(const WorkMeta &work) const;
    QString badgeFor(const WorkMeta &work) const; // "+N гл." / "обновлено" for downloaded books with news
    bool siteNewer(const WorkMeta &work) const;

    // A tile wired to openBook / action and kept up to date as details arrive.
    // `fixedBadge` replaces the update badge (e.g. the number in a series).
    Tile *makeTile(const WorkMeta &work, QWidget *parent, const QString &fixedBadge = QString(), bool showAuthor = false);
    // Ask StatusCache for details of these books (background, rate-limited).
    void requestStatus(const QList<WorkMeta> &works);

    void emitStatusChanged(qint64 id) { emit statusChanged(id); }

    // Genre titles from /v1/work/genres, loaded once and cached on disk.
    QString genreNames(const QList<int> &ids) const;
    void loadGenres(const QString &cacheFile);
    // Again, when the first load ran before the network was up.
    void reloadGenres();
    // "Reconnect Wi-Fi if the firmware switched it off" for lists that load on demand.
    // Search, sync and settings on the right of a header: the same on every screen.
    void addHeaderButtons(TopBar *top, bool withSettings = true);

    // Every network call goes through this: it brings Wi-Fi up, once at a time, with a timeout.
    WorkList::NetworkGate networkGate();

signals:
    void openBook(qint64 id);
    void action(qint64 id, ActionState state);
    void statusChanged(qint64 id); // details arrived or a download finished
    void libraryChanged();
    void openSettings();
    void openSearch();
    void openAuthor(const QString &login, const QString &name);
    void openTag(const QString &tag); // catalog tab filtered by this exact tag
    void syncRequested();

private:
    std::unique_ptr<NetworkGate> m_gate;
    QString m_genresFile;
};
