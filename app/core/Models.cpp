#include "Models.h"

#include <QJsonArray>

namespace {

QDateTime time(const QJsonObject &o, const char *key)
{
    return QDateTime::fromString(o.value(QLatin1String(key)).toString(), Qt::ISODate);
}

qint64 toId(const QJsonValue &v)
{
    return qint64(v.toDouble());
}

} // namespace

WorkMeta WorkMeta::fromJson(const QJsonObject &o)
{
    WorkMeta m;
    m.id = toId(o.value(QStringLiteral("id")));
    m.title = o.value(QStringLiteral("title")).toString();
    QStringList names;
    for (const char *key : {"authorFIO", "coAuthorFIO", "secondCoAuthorFIO"}) {
        const QString n = o.value(QLatin1String(key)).toString();
        if (!n.isEmpty())
            names << n;
    }
    // toJson() stores the joined form under "authors".
    m.authors = names.isEmpty() ? o.value(QStringLiteral("authors")).toString() : names.join(QStringLiteral(", "));
    m.authorLogin = o.value(QStringLiteral("authorUserName")).toString();
    m.coverUrl = o.value(QStringLiteral("coverUrl")).toString();
    // Some lists (atRecommendations) send paths relative to the content host.
    if (!m.coverUrl.isEmpty() && !m.coverUrl.startsWith(QLatin1String("http")))
        m.coverUrl = QStringLiteral("https://cm.author.today/content/") + m.coverUrl;
    // Catalog items call it workInLibraryState; tile views say "finished" instead of "isFinished".
    m.libraryState = o.contains(QStringLiteral("inLibraryState")) ? o.value(QStringLiteral("inLibraryState")).toString()
                                                                  : o.value(QStringLiteral("workInLibraryState")).toString();
    m.lastModificationTime = time(o, "lastModificationTime");
    m.isPurchased = o.value(QStringLiteral("isPurchased")).toBool();
    m.isFinished = o.contains(QStringLiteral("isFinished")) ? o.value(QStringLiteral("isFinished")).toBool()
                                                            : o.value(QStringLiteral("finished")).toBool();
    m.price = o.value(QStringLiteral("price")).toDouble();
    m.seriesId = toId(o.value(QStringLiteral("seriesId")));
    m.seriesTitle = o.value(QStringLiteral("seriesTitle")).toString();
    m.adultOnly = o.value(QStringLiteral("adultOnly")).toBool();
    m.usingNeuralNetworks = o.value(QStringLiteral("usingNeuralNetworks")).toBool();
    m.likeCount = o.value(QStringLiteral("likeCount")).toInt();
    m.textLength = qint64(o.value(QStringLiteral("textLength")).toDouble());
    m.lastUpdateTime = time(o, "lastUpdateTime");
    if (o.contains(QStringLiteral("genreIds"))) {
        for (const QJsonValue &v : o.value(QStringLiteral("genreIds")).toArray())
            m.genreIds << v.toInt();
    } else {
        for (const char *key : {"genreId", "firstSubGenreId", "secondSubGenreId"}) {
            const int g = o.value(QLatin1String(key)).toInt();
            if (g > 0)
                m.genreIds << g;
        }
    }
    return m;
}

QJsonObject WorkMeta::toJson() const
{
    return QJsonObject{
        {QStringLiteral("id"), double(id)},
        {QStringLiteral("title"), title},
        {QStringLiteral("authors"), authors},
        {QStringLiteral("authorUserName"), authorLogin},
        {QStringLiteral("coverUrl"), coverUrl},
        {QStringLiteral("inLibraryState"), libraryState},
        {QStringLiteral("lastModificationTime"), lastModificationTime.toUTC().toString(Qt::ISODate)},
        {QStringLiteral("isPurchased"), isPurchased},
        {QStringLiteral("isFinished"), isFinished},
        {QStringLiteral("price"), price},
        {QStringLiteral("seriesId"), double(seriesId)},
        {QStringLiteral("seriesTitle"), seriesTitle},
        {QStringLiteral("adultOnly"), adultOnly},
        {QStringLiteral("usingNeuralNetworks"), usingNeuralNetworks},
        {QStringLiteral("likeCount"), likeCount},
        {QStringLiteral("textLength"), double(textLength)},
        {QStringLiteral("lastUpdateTime"), lastUpdateTime.toUTC().toString(Qt::ISODate)},
        {QStringLiteral("genreIds"), [this] {
             QJsonArray a;
             for (int g : genreIds)
                 a.append(g);
             return a;
         }()},
    };
}

WorkDetails WorkDetails::fromJson(const QJsonObject &o)
{
    WorkDetails d;
    d.meta = WorkMeta::fromJson(o);
    d.annotation = o.value(QStringLiteral("annotation")).toString();
    d.allowDownloads = o.value(QStringLiteral("allowDownloads")).toBool();
    d.privacyDownloads = o.value(QStringLiteral("privacyDownloads")).toString();
    d.downloadErrorMessage = o.value(QStringLiteral("downloadErrorMessage")).toString();
    for (const QJsonValue &v : o.value(QStringLiteral("tags")).toArray())
        d.tags << v.toString();
    for (const QJsonValue &v : o.value(QStringLiteral("seriesWorkIds")).toArray())
        d.seriesWorkIds << toId(v);
    d.seriesWorkNumber = o.value(QStringLiteral("seriesWorkNumber")).toInt();
    d.seriesPurchaseIsAllowed = o.value(QStringLiteral("seriesPurchaseIsAllowed")).toBool();
    d.lastChapterId = toId(o.value(QStringLiteral("lastChapterId")));
    d.lastChapterProgress = o.value(QStringLiteral("lastChapterProgress")).toDouble();
    d.lastReadTime = time(o, "lastReadTime");
    for (const QJsonValue &v : o.value(QStringLiteral("chapters")).toArray()) {
        const QJsonObject c = v.toObject();
        Chapter ch;
        ch.id = toId(c.value(QStringLiteral("id")));
        ch.title = c.value(QStringLiteral("title")).toString();
        ch.publishTime = time(c, "publishTime");
        ch.lastModificationTime = time(c, "lastModificationTime");
        ch.isDraft = c.value(QStringLiteral("isDraft")).toBool();
        ch.isAvailable = c.value(QStringLiteral("isAvailable")).toBool(true);
        ch.sortOrder = c.value(QStringLiteral("sortOrder")).toInt();
        ch.textLength = qint64(c.value(QStringLiteral("textLength")).toDouble());
        d.chapters << ch;
    }
    d.chapterCount = qMax(d.chapters.size(), o.value(QStringLiteral("chapterCount")).toInt());
    return d;
}

QJsonObject WorkDetails::toJson() const
{
    QJsonObject o = meta.toJson();
    o.insert(QStringLiteral("annotation"), annotation);
    o.insert(QStringLiteral("allowDownloads"), allowDownloads);
    o.insert(QStringLiteral("privacyDownloads"), privacyDownloads);
    o.insert(QStringLiteral("downloadErrorMessage"), downloadErrorMessage);
    o.insert(QStringLiteral("tags"), QJsonArray::fromStringList(tags));
    QJsonArray series;
    for (qint64 id : seriesWorkIds)
        series.append(double(id));
    o.insert(QStringLiteral("seriesWorkIds"), series);
    o.insert(QStringLiteral("seriesWorkNumber"), seriesWorkNumber);
    o.insert(QStringLiteral("seriesPurchaseIsAllowed"), seriesPurchaseIsAllowed);
    o.insert(QStringLiteral("lastChapterId"), double(lastChapterId));
    o.insert(QStringLiteral("lastChapterProgress"), lastChapterProgress);
    if (lastReadTime.isValid())
        o.insert(QStringLiteral("lastReadTime"), lastReadTime.toUTC().toString(Qt::ISODate));
    QJsonArray chs;
    for (const Chapter &c : chapters) {
        chs.append(QJsonObject{{QStringLiteral("id"), double(c.id)},
                               {QStringLiteral("title"), c.title},
                               {QStringLiteral("publishTime"), c.publishTime.toUTC().toString(Qt::ISODate)},
                               {QStringLiteral("lastModificationTime"), c.lastModificationTime.toUTC().toString(Qt::ISODate)},
                               {QStringLiteral("isDraft"), c.isDraft},
                               {QStringLiteral("isAvailable"), c.isAvailable},
                               {QStringLiteral("sortOrder"), c.sortOrder},
                               {QStringLiteral("textLength"), double(c.textLength)}});
    }
    o.insert(QStringLiteral("chapters"), chs);
    return o;
}

QHash<int, QString> parseGenreNames(const QJsonArray &genres)
{
    QHash<int, QString> names;
    for (const QJsonValue &v : genres) {
        const QJsonObject o = v.toObject();
        names.insert(o.value(QStringLiteral("id")).toInt(), o.value(QStringLiteral("title")).toString());
    }
    return names;
}

QList<Genre> parseGenres(const QJsonArray &genres)
{
    QList<Genre> list;
    for (const QJsonValue &v : genres) {
        const QJsonObject o = v.toObject();
        Genre g;
        g.id = o.value(QStringLiteral("id")).toInt();
        g.parentId = o.value(QStringLiteral("parentId")).toInt(); // null -> 0
        g.title = o.value(QStringLiteral("title")).toString();
        g.code = o.value(QStringLiteral("code")).toString();
        g.workCount = o.value(QStringLiteral("workCount")).toInt();
        list << g;
    }
    return list;
}

QJsonObject WorkDetails::toCompactJson() const
{
    QJsonObject o = toJson();
    o.remove(QStringLiteral("chapters"));
    o.remove(QStringLiteral("annotation"));
    o.insert(QStringLiteral("chapterCount"), chapterTotal());
    return o;
}
