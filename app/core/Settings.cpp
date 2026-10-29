#include "Settings.h"

#include <QFile>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include "FileIO.h"

Settings::Settings(const QString &filePath)
    : m_path(filePath)
{
    QFile f(m_path);
    if (!f.open(QIODevice::ReadOnly))
        return;
    const QJsonObject o = QJsonDocument::fromJson(f.readAll()).object();
    auto readBool = [&](const char *key, bool &field) {
        if (o.contains(QLatin1String(key)))
            field = o.value(QLatin1String(key)).toBool();
    };
    if (o.contains(QStringLiteral("homeShelves"))) {
        homeShelves.clear();
        for (const QJsonValue &v : o.value(QStringLiteral("homeShelves")).toArray())
            homeShelves << v.toString();
    }
    downloadFormat = o.value(QStringLiteral("downloadFormat")).toString(downloadFormat);
    storageRoot = o.value(QStringLiteral("storageRoot")).toString(storageRoot);
    readBool("openAfterDownload", openAfterDownload);
    readBool("sendPositionToSite", sendPositionToSite);
    readBool("openAtSiteChapter", openAtSiteChapter);
    readBool("checkUpdates", checkUpdates);
    readBool("onlyDownloadable", onlyDownloadable);
    readBool("hideAdult", hideAdult);
    readBool("hideAi", hideAi);
    readBool("loadCovers", loadCovers);
}

void Settings::save() const
{
    const QJsonObject o{
        {QStringLiteral("homeShelves"), QJsonArray::fromStringList(homeShelves)},
        {QStringLiteral("downloadFormat"), downloadFormat},
        {QStringLiteral("storageRoot"), storageRoot},
        {QStringLiteral("openAfterDownload"), openAfterDownload},
        {QStringLiteral("sendPositionToSite"), sendPositionToSite},
        {QStringLiteral("openAtSiteChapter"), openAtSiteChapter},
        {QStringLiteral("checkUpdates"), checkUpdates},
        {QStringLiteral("onlyDownloadable"), onlyDownloadable},
        {QStringLiteral("hideAdult"), hideAdult},
        {QStringLiteral("hideAi"), hideAi},
        {QStringLiteral("loadCovers"), loadCovers},
    };
    writeFileIfChanged(m_path, QJsonDocument(o).toJson());
}
