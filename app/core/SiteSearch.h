#pragma once

#include <QByteArray>
#include <QList>
#include <QString>
#include <QUrl>

// Text search and author pages exist only on the site (the API has neither): build the
// addresses and read book ids from the HTML. The markers match the site as of October 2026.

QByteArray siteUserAgent();                          // mobile browser UA plus InkToday/<version>
QUrl siteSearchUrl(const QString &text, int page);   // works, e-books only, page from 1
QUrl siteAuthorsUrl(const QString &text);
QUrl authorWorksUrl(const QString &login, int page); // e-books only

struct SitePage {
    bool recognized = false; // false: not a search/author page (DDoS-Guard check, new layout)
    QList<qint64> ids;       // e-book ids in page order; audiobooks are skipped
    int total = -1;          // "Найдено: N"; -1 when the page does not say
    bool hasNext = false;
};
SitePage parseSiteWorks(const QByteArray &html, int page);
SitePage parseAuthorWorks(const QByteArray &html, const QString &login, int page);

struct SiteAuthor {
    QString login;
    QString name;
    int works = 0;
};
QList<SiteAuthor> parseSiteAuthors(const QByteArray &html, bool *recognized);
