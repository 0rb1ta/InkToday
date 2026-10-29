#include "AppContext.h"

#include "NetworkGate.h"

#include "ApiClient.h"
#include "AuthService.h"
#include "Device.h"
#include "DownloadIndex.h"
#include "Settings.h"
#include "StatusCache.h"
#include "Widgets.h"

#include <QPushButton>

#include <QFile>
#include <QJsonArray>
#include <QJsonDocument>
#include <QPointer>
#include <QSaveFile>
#include <QTimer>

bool AppContext::siteNewer(const WorkMeta &work) const
{
    const QDateTime site = status->has(work.id) ? status->get(work.id).meta.lastModificationTime : work.lastModificationTime;
    return index->hasUpdate(work.id, site);
}

ActionState AppContext::stateFor(const WorkMeta &work) const
{
    BookFacts f;
    f.loggedIn = auth->isLoggedIn();
    f.haveDetails = status->has(work.id);
    const WorkDetails d = status->get(work.id);
    f.allowDownloads = f.haveDetails && d.allowDownloads;
    f.subscribersOnly = f.haveDetails && d.privacyDownloads == QLatin1String("OnlySubscribers");
    f.isPurchased = work.isPurchased || (f.haveDetails && d.meta.isPurchased);
    f.price = f.haveDetails ? d.meta.price : work.price;
    f.downloaded = index->contains(work.id);
    f.fileMissing = index->fileMissing(work.id);
    f.siteNewer = siteNewer(work);
    return resolveAction(f);
}

QString AppContext::badgeFor(const WorkMeta &work) const
{
    BookFacts f;
    f.downloaded = index->contains(work.id);
    f.fileMissing = index->fileMissing(work.id);
    f.siteNewer = f.downloaded && siteNewer(work);
    f.haveDetails = status->has(work.id);
    f.allowDownloads = f.haveDetails && status->get(work.id).allowDownloads;
    const int n = f.downloaded && f.haveDetails ? newChapters(index->get(work.id), status->get(work.id)) : 0;
    return updateBadge(f, n);
}

Tile *AppContext::makeTile(const WorkMeta &work, QWidget *parent, const QString &fixedBadge, bool showAuthor)
{
    auto *tile = new Tile(covers, settings->loadCovers, parent);
    tile->setShowAuthor(showAuthor);
    tile->setWork(work, stateFor(work), fixedBadge.isEmpty() ? badgeFor(work) : fixedBadge);
    connect(tile, &Tile::opened, this, &AppContext::openBook);
    connect(tile, &Tile::actionClicked, this, &AppContext::action);
    QPointer<Tile> guard(tile);
    const WorkMeta copy = work;
    connect(this, &AppContext::statusChanged, tile, [this, guard, copy, fixedBadge](qint64 id) {
        if (guard && id == copy.id)
            guard->setWork(copy, stateFor(copy), fixedBadge.isEmpty() ? badgeFor(copy) : fixedBadge);
    });
    return tile;
}

void AppContext::requestStatus(const QList<WorkMeta> &works)
{
    QList<qint64> ids;
    for (const WorkMeta &w : works)
        ids << w.id;
    status->request(ids);
}

QString AppContext::genreNames(const QList<int> &ids) const
{
    QStringList names;
    for (int id : ids)
        if (genres.contains(id))
            names << genres.value(id);
    return names.join(QStringLiteral(", "));
}

void AppContext::reloadGenres()
{
    if (genreList.isEmpty() && !m_genresFile.isEmpty())
        loadGenres(m_genresFile);
}

void AppContext::loadGenres(const QString &cacheFile)
{
    m_genresFile = cacheFile;
    QFile f(cacheFile);
    if (f.open(QIODevice::ReadOnly)) {
        const QJsonArray all = QJsonDocument::fromJson(f.readAll()).array();
        genres = parseGenreNames(all);
        genreList = parseGenres(all);
    }
    if (!genres.isEmpty())
        return;
    client->get(QStringLiteral("/v1/work/genres"), [this, cacheFile](const QJsonDocument &doc, const ApiError *e) {
        if (e || !doc.isArray())
            return;
        genres = parseGenreNames(doc.array());
        genreList = parseGenres(doc.array());
        QSaveFile out(cacheFile);
        if (out.open(QIODevice::WriteOnly)) {
            out.write(QJsonDocument(doc.array()).toJson(QJsonDocument::Compact));
            out.commit();
        }
    });
}

AppContext::AppContext() = default;
AppContext::~AppContext() = default;

void AppContext::addHeaderButtons(TopBar *top, bool withSettings)
{
    connect(top->addIconButton(QStringLiteral("magnifying-glass")), &QPushButton::clicked, this, &AppContext::openSearch);
    connect(top->addIconButton(QStringLiteral("rotate")), &QPushButton::clicked, this, &AppContext::syncRequested);
    if (withSettings)
        connect(top->addIconButton(QStringLiteral("gear")), &QPushButton::clicked, this, &AppContext::openSettings);
}

WorkList::NetworkGate AppContext::networkGate()
{
    if (!m_gate)
        m_gate.reset(new NetworkGate([] { return device::networkConnected(); },
                                     [](std::function<void(bool)> done) { device::connectNetworkAsync(std::move(done)); }));
    const NetworkGate gate = *m_gate;
    return [gate](std::function<void(bool)> done) { gate.run(std::move(done)); };
}
