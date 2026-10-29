#pragma once

#include <QDateTime>
#include <QJsonObject>
#include <QList>
#include <QHash>
#include <QJsonArray>
#include <QString>
#include <QStringList>

// Fields of the API's WorkMetaInfo / WorkDetails that the app uses (field names as in the API's Swagger schema).
struct WorkMeta {
    qint64 id = 0;
    QString title;
    QString authors; // authorFIO, then co-authors, joined with ", "
    QString authorLogin; // authorUserName: the site address /u/<login> of the first author
    QString coverUrl;
    QString libraryState; // Reading / Saved / Finished / Disliked / None
    QDateTime lastModificationTime;
    bool isPurchased = false;
    bool isFinished = false;
    double price = 0;
    qint64 seriesId = 0;
    QString seriesTitle;
    bool adultOnly = false;
    bool usingNeuralNetworks = false;
    int likeCount = 0;
    qint64 textLength = 0;           // characters
    QDateTime lastUpdateTime;        // last chapter published / text changed
    QList<int> genreIds;             // genreId, firstSubGenreId, secondSubGenreId (non-zero)

    static WorkMeta fromJson(const QJsonObject &o);
    QJsonObject toJson() const;
};

struct Chapter {
    qint64 id = 0;
    QString title;
    QDateTime publishTime;
    QDateTime lastModificationTime;
    bool isDraft = false;
    bool isAvailable = true;
    int sortOrder = 0;      // reading order on the site
    qint64 textLength = 0;  // characters
};

struct WorkDetails {
    WorkMeta meta;
    QString annotation;
    bool allowDownloads = false;
    QString privacyDownloads;
    QString downloadErrorMessage;
    QStringList tags; // exact tag names: catalog/search ?tag=
    QList<qint64> seriesWorkIds;
    int seriesWorkNumber = 0;
    bool seriesPurchaseIsAllowed = false;
    qint64 lastChapterId = 0;        // where the reader stopped on the site (0: never read there)
    double lastChapterProgress = 0;  // percent of that chapter, 0...100
    QDateTime lastReadTime;          // when it was last read on the site (null: unknown)
    QList<Chapter> chapters;
    int chapterCount = 0; // known count when the list itself was not kept (compact cache)

    int chapterTotal() const { return qMax(chapters.size(), chapterCount); }

    static WorkDetails fromJson(const QJsonObject &o);
    QJsonObject toJson() const; // round-trips through fromJson
    QJsonObject toCompactJson() const; // no chapter list / annotation: what the status cache stores
};

// /v1/work/genres -> id -> title
QHash<int, QString> parseGenreNames(const QJsonArray &genres);

struct Genre {
    int id = 0;
    int parentId = 0; // 0: top level
    QString title;
    QString code; // catalog/search ?genre=<code>
    int workCount = 0;
};
// /v1/work/genres in answer order.
QList<Genre> parseGenres(const QJsonArray &genres);
