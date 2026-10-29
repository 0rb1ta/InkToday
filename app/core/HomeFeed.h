#pragma once

#include "Models.h"

#include <QJsonObject>
#include <QObject>
#include <QPair>

#include <functional>

class ApiClient;
class Settings;
struct ApiError;

struct Shelf {
    QString key;   // API field of /v1/home/home-page
    QString title; // Russian heading
    QList<WorkMeta> works;
};

// The 8 home-page lists in display order.
const QList<QPair<QString, QString>> &allShelves();

// Enabled shelves only, in allShelves() order; adult / AI books dropped per settings.
QList<Shelf> parseHomeFeed(const QJsonObject &homePage, const Settings &settings);

// Fetches /v1/home/home-page and keeps the last answer on disk for offline start.
class HomeFeed : public QObject {
    Q_OBJECT
public:
    HomeFeed(ApiClient *client, Settings *settings, const QString &cacheFile, QObject *parent = nullptr);

    QList<Shelf> shelves() const;
    void refresh(std::function<void(const ApiError *)> done);

private:
    ApiClient *m_client;
    Settings *m_settings;
    QString m_cacheFile;
    QJsonObject m_last;
};
