#include "Catalog.h"

#include <QJsonArray>
#include <QUrl>
#include <QUrlQuery>

QString catalogPath(const CatalogQuery &q, int page, int pageSize)
{
    QUrlQuery u;
    u.addQueryItem(QStringLiteral("page"), QString::number(page));
    u.addQueryItem(QStringLiteral("ps"), QString::number(pageSize));
    u.addQueryItem(QStringLiteral("sorting"), q.sorting);
    if (!q.genre.isEmpty())
        u.addQueryItem(QStringLiteral("genre"), q.genre);
    if (q.state != QLatin1String("any"))
        u.addQueryItem(QStringLiteral("state"), q.state);
    if (q.access != QLatin1String("any"))
        u.addQueryItem(QStringLiteral("access"), q.access);
    if (q.onlyDownloadable)
        u.addQueryItem(QStringLiteral("dnl"), QStringLiteral("allowed"));
    if (q.hideAi)
        u.addQueryItem(QStringLiteral("usingNeuralNetworks"), QStringLiteral("false"));
    if (!q.tag.isEmpty())
        u.addQueryItem(QStringLiteral("tag"), QString::fromLatin1(QUrl::toPercentEncoding(q.tag)));
    return QStringLiteral("/v1/catalog/search?") + u.toString(QUrl::FullyEncoded);
}

CatalogPage parseCatalogPage(const QJsonObject &o)
{
    CatalogPage p;
    for (const QJsonValue &v : o.value(QStringLiteral("searchResults")).toArray())
        p.works << WorkMeta::fromJson(v.toObject());
    p.total = o.value(QStringLiteral("realTotalCount")).toInt();
    p.isLast = o.value(QStringLiteral("isLastPage")).toBool(true);
    return p;
}

const QList<QPair<QString, QString>> &catalogSortings()
{
    static const QList<QPair<QString, QString>> list{
        {QStringLiteral("popular"), QStringLiteral("по популярности")},
        {QStringLiteral("recent"), QStringLiteral("по новизне")},
        {QStringLiteral("likes"), QStringLiteral("по лайкам")},
        {QStringLiteral("views"), QStringLiteral("по просмотрам")},
        {QStringLiteral("comments"), QStringLiteral("по комментариям")},
        {QStringLiteral("length"), QStringLiteral("по объёму")},
        {QStringLiteral("trending"), QStringLiteral("набирающие популярность")},
    };
    return list;
}

const QList<QPair<QString, QString>> &catalogStates()
{
    static const QList<QPair<QString, QString>> list{
        {QStringLiteral("any"), QStringLiteral("любой")},
        {QStringLiteral("in-progress"), QStringLiteral("в процессе")},
        {QStringLiteral("finished"), QStringLiteral("завершено")},
    };
    return list;
}

const QList<QPair<QString, QString>> &catalogAccesses()
{
    static const QList<QPair<QString, QString>> list{
        {QStringLiteral("any"), QStringLiteral("любой")},
        {QStringLiteral("free"), QStringLiteral("бесплатные")},
        {QStringLiteral("paid"), QStringLiteral("платные")},
    };
    return list;
}
