#pragma once

#include <QString>
#include <QStringList>

// User settings, stored as JSON. Fields are public: screens edit them and call save().
class Settings {
public:
    explicit Settings(const QString &filePath);

    QStringList homeShelves{QStringLiteral("atRecommendations"), QStringLiteral("bestsellerWorks")};
    QString downloadFormat = QStringLiteral("Epub");
    bool openAfterDownload = true;
    bool sendPositionToSite = false; // reader -> site, only forward
    bool openAtSiteChapter = false;  // "Читать" offers the place last read on the site when it is newer
    bool checkUpdates = true;
    bool onlyDownloadable = true;
    bool hideAdult = false;
    bool hideAi = false;
    bool loadCovers = true;
    QString storageRoot = QStringLiteral("/mnt/ext1");

    void save() const;

private:
    QString m_path;
};
