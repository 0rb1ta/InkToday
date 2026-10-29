#pragma once

#include "Models.h"

#include <QJsonObject>
#include <QList>
#include <QPair>
#include <QString>

// Filters of GET /v1/catalog/search that the catalog screen offers (values checked against the
// /v1/catalog/* dictionaries).
struct CatalogQuery {
    QString tag;                    // exact tag title, empty: no tag
    QString genre;                  // genre code, empty: all genres
    QString sorting = QStringLiteral("popular");
    QString state = QStringLiteral("any");  // any / in-progress / finished
    QString access = QStringLiteral("any"); // any / free / paid
    bool onlyDownloadable = true;   // dnl=allowed; otherwise not sent
    bool hideAi = false;            // usingNeuralNetworks=false
};

QString catalogPath(const CatalogQuery &q, int page, int pageSize); // "/v1/catalog/search?..."

struct CatalogPage {
    QList<WorkMeta> works;
    int total = 0;
    bool isLast = true;
};
CatalogPage parseCatalogPage(const QJsonObject &o);

// Values and Russian captions for the filter buttons, in display order.
const QList<QPair<QString, QString>> &catalogSortings();
const QList<QPair<QString, QString>> &catalogStates();
const QList<QPair<QString, QString>> &catalogAccesses();
