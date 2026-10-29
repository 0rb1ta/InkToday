#include "SiteSearch.h"

#include <QRegularExpression>
#include <QUrlQuery>

namespace {

const QString kSite = QStringLiteral("https://author.today");

QUrl searchUrl(const QString &category, const QString &text, int page, bool ebookOnly)
{
    QUrl url(kSite + QStringLiteral("/search"));
    QUrlQuery q;
    q.addQueryItem(QStringLiteral("category"), category);
    q.addQueryItem(QStringLiteral("q"), QString::fromLatin1(QUrl::toPercentEncoding(text)));
    if (ebookOnly)
        q.addQueryItem(QStringLiteral("format"), QStringLiteral("ebook"));
    // Without page= the site answers 301 to the same address.
    q.addQueryItem(QStringLiteral("page"), QString::number(page));
    url.setQuery(q.query(QUrl::FullyEncoded), QUrl::StrictMode);
    return url;
}

// "Найдено: 4 812" or, since October 2026, "Результатов: 4 812" (spaces may be no-break) -> 4812,
// -1 if absent.
int foundCount(const QString &s)
{
    static const QRegularExpression re(QStringLiteral("(?:Найдено|Результатов):\\s*([\\d\\s\\x{00A0}]+)"));
    const auto m = re.match(s);
    if (!m.hasMatch())
        return -1;
    QString digits = m.captured(1);
    digits.remove(QRegularExpression(QStringLiteral("[^\\d]")));
    return digits.toInt();
}

QList<qint64> workRows(const QString &s)
{
    // A result row links its cover: "work-row" before October 2026, "book-cover-content" since.
    static const QRegularExpression re(QStringLiteral("href=\"/(work|audiobook)/(\\d+)\"\\s+class=\"(?:work-row|book-cover-content)"));
    QList<qint64> ids;
    auto it = re.globalMatch(s);
    while (it.hasNext()) {
        const auto m = it.next();
        if (m.captured(1) == QLatin1String("work") && !ids.contains(m.captured(2).toLongLong()))
            ids << m.captured(2).toLongLong();
    }
    return ids;
}

bool linksToPage(const QString &s, int page)
{
    return QRegularExpression(QStringLiteral("[?&;]page=%1[\"&]").arg(page)).match(s).hasMatch();
}

QString plainText(QString s)
{
    s.remove(QRegularExpression(QStringLiteral("<[^>]*>")));
    s.replace(QStringLiteral("&quot;"), QStringLiteral("\""));
    s.replace(QStringLiteral("&#39;"), QStringLiteral("'"));
    s.replace(QStringLiteral("&lt;"), QStringLiteral("<"));
    s.replace(QStringLiteral("&gt;"), QStringLiteral(">"));
    s.replace(QStringLiteral("&amp;"), QStringLiteral("&"));
    return s.simplified();
}

} // namespace

QByteArray siteUserAgent()
{
    return QByteArrayLiteral("Mozilla/5.0 (Linux; Android 10; PocketBook) AppleWebKit/537.36 (KHTML, like Gecko) "
                             "Chrome/120 Mobile Safari/537.36 InkToday/" APP_VERSION);
}

QUrl siteSearchUrl(const QString &text, int page)
{
    return searchUrl(QStringLiteral("works"), text, page, true);
}

QUrl siteAuthorsUrl(const QString &text)
{
    return searchUrl(QStringLiteral("authors"), text, 1, false);
}

QUrl authorWorksUrl(const QString &login, int page)
{
    return QUrl(kSite + QStringLiteral("/u/%1/works?format=ebook&page=%2").arg(login).arg(page));
}

SitePage parseSiteWorks(const QByteArray &html, int page)
{
    const QString s = QString::fromUtf8(html);
    SitePage p;
    p.total = foundCount(s);
    if (p.total < 0 && s.contains(QStringLiteral("не дал результатов")))
        p.total = 0;
    p.recognized = p.total >= 0;
    if (!p.recognized)
        return p;
    p.ids = workRows(s);
    p.hasNext = !p.ids.isEmpty() && linksToPage(s, page + 1);
    return p;
}

SitePage parseAuthorWorks(const QByteArray &html, const QString &login, int page)
{
    const QString s = QString::fromUtf8(html);
    SitePage p;
    p.recognized = s.contains(QStringLiteral("/u/%1/works").arg(login));
    if (!p.recognized)
        return p;
    p.ids = workRows(s);
    p.hasNext = linksToPage(s, page + 1);
    return p;
}

QList<SiteAuthor> parseSiteAuthors(const QByteArray &html, bool *recognized)
{
    const QString s = QString::fromUtf8(html);
    QList<SiteAuthor> list;
    *recognized = foundCount(s) >= 0 || s.contains(QStringLiteral("не дал результатов"));
    if (!*recognized)
        return list;
    // One card per author: <a href=/u/<login> class="card-content"> … profile-name … "Произведений: N".
    static const QRegularExpression card(QStringLiteral(
        "<a href=\"?/u/([^\\s>\"]+)\"? class=\"card-content\">.*?<div class=\"profile-name\">\\s*<span class=\"text-truncate\">(.*?)</span>(.*?)</a>"),
        QRegularExpression::DotMatchesEverythingOption);
    static const QRegularExpression works(QStringLiteral("Произведений:\\s*(\\d+)"));
    auto it = card.globalMatch(s);
    while (it.hasNext()) {
        const auto m = it.next();
        SiteAuthor a;
        a.login = m.captured(1);
        a.name = plainText(m.captured(2));
        a.works = works.match(m.captured(3)).captured(1).toInt();
        list << a;
    }
    return list;
}
