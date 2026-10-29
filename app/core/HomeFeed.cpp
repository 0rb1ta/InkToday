#include "HomeFeed.h"

#include "ApiClient.h"
#include "Settings.h"

#include <QFile>
#include <QJsonArray>
#include <QJsonDocument>
#include "FileIO.h"

const QList<QPair<QString, QString>> &allShelves()
{
    static const QList<QPair<QString, QString>> shelves{
        {QStringLiteral("atRecommendations"), QStringLiteral("АТ рекомендует")},
        {QStringLiteral("bestsellerWorks"), QStringLiteral("Бестселлеры")},
        {QStringLiteral("mostPopularWorks"), QStringLiteral("Популярное")},
        {QStringLiteral("hotWorks"), QStringLiteral("Горячие новинки")},
        {QStringLiteral("recentPubWorks"), QStringLiteral("Новые публикации")},
        {QStringLiteral("recentUpdWorks"), QStringLiteral("Недавно обновлённые")},
        {QStringLiteral("addedToLibraryWorks"), QStringLiteral("Сейчас добавляют в библиотеки")},
        {QStringLiteral("recentLikedWorks"), QStringLiteral("Недавно понравившиеся")},
    };
    return shelves;
}

QList<Shelf> parseHomeFeed(const QJsonObject &homePage, const Settings &settings)
{
    QList<Shelf> result;
    for (const auto &entry : allShelves()) {
        if (!settings.homeShelves.contains(entry.first))
            continue;
        Shelf shelf{entry.first, entry.second, {}};
        for (const QJsonValue &v : homePage.value(entry.first).toArray()) {
            const WorkMeta w = WorkMeta::fromJson(v.toObject());
            if ((settings.hideAdult && w.adultOnly) || (settings.hideAi && w.usingNeuralNetworks))
                continue;
            shelf.works << w;
        }
        result << shelf;
    }
    return result;
}

HomeFeed::HomeFeed(ApiClient *client, Settings *settings, const QString &cacheFile, QObject *parent)
    : QObject(parent)
    , m_client(client)
    , m_settings(settings)
    , m_cacheFile(cacheFile)
{
    QFile f(m_cacheFile);
    if (f.open(QIODevice::ReadOnly))
        m_last = QJsonDocument::fromJson(f.readAll()).object();
}

QList<Shelf> HomeFeed::shelves() const
{
    return parseHomeFeed(m_last, *m_settings);
}

void HomeFeed::refresh(std::function<void(const ApiError *)> done)
{
    m_client->get(QStringLiteral("/v1/home/home-page"), [this, done](const QJsonDocument &doc, const ApiError *e) {
        if (!e && doc.isObject()) {
            m_last = doc.object();
            writeFileIfChanged(m_cacheFile, QJsonDocument(m_last).toJson(QJsonDocument::Compact));
        }
        done(e);
    });
}
