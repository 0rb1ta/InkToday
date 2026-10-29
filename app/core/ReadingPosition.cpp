#include "ReadingPosition.h"

#include <QRegularExpression>
#include <QXmlStreamReader>

#include <algorithm>

namespace {

QString norm(const QString &title)
{
    return title.simplified().toCaseFolded();
}

} // namespace

ChapterMap mapChapters(const QList<EpubNavPoint> &toc, const QList<Chapter> &siteChapters, int spineCount)
{
    QList<Chapter> site = siteChapters;
    std::stable_sort(site.begin(), site.end(), [](const Chapter &a, const Chapter &b) { return a.sortOrder < b.sortOrder; });
    ChapterMap map;
    const int n = site.size();
    if (n == 0)
        return map;
    // The site's chapters as one run of titles in the toc. The run may stop at the end of the toc:
    // a serial gets new chapters on the site after the download, and those stay unmapped.
    int start = -1, matched = 0;
    for (int k = 0; k < toc.size() && start < 0; ++k) {
        const int m = qMin(n, toc.size() - k);
        bool same = true;
        for (int i = 0; i < m && same; ++i)
            same = norm(toc[k + i].title) == norm(site[i].title);
        if (same) {
            start = k;
            matched = m;
        }
    }
    map.exact = start >= 0;
    if (start < 0) {
        if (toc.size() < n)
            return map;
        start = toc.size() - n; // titles changed since download: same order, counted from the end
        matched = n;
    }
    for (int i = 0; i < matched; ++i) {
        // Files without a toc entry after a chapter's start are the rest of that chapter.
        const int first = toc[start + i].spineIndex;
        const int end = start + i + 1 < toc.size() ? toc[start + i + 1].spineIndex : qMax(spineCount, first + 1);
        map.spineByChapter.insert(site[i].id, first);
        for (int spine = first; spine < qMax(end, first + 1); ++spine) {
            map.chapterBySpine.insert(spine, site[i].id);
            map.spinesByChapter[site[i].id] << spine;
        }
    }
    return map;
}

double groupPercent(const QList<int> &spines, const QList<int> &textLengths, int spine, double filePercent)
{
    double before = 0, total = 0;
    for (int s : spines) {
        const double len = s >= 0 && s < textLengths.size() ? textLengths[s] : 0;
        if (s < spine)
            before += len;
        total += len;
    }
    if (total <= 0 || !spines.contains(spine))
        return filePercent;
    const double len = spine < textLengths.size() ? textLengths[spine] : 0;
    return 100.0 * (before + len * filePercent / 100.0) / total;
}

QPair<int, double> placeInGroup(const QList<int> &spines, const QList<int> &textLengths, double groupPercent)
{
    if (spines.isEmpty())
        return qMakePair(-1, 0.0);
    double total = 0;
    for (int s : spines)
        total += s >= 0 && s < textLengths.size() ? textLengths[s] : 0;
    if (total <= 0 || spines.size() == 1)
        return qMakePair(spines.first(), groupPercent);
    double left = total * qBound(0.0, groupPercent, 100.0) / 100.0;
    for (int s : spines) {
        const double len = s >= 0 && s < textLengths.size() ? textLengths[s] : 0;
        if (left <= len || s == spines.last())
            return qMakePair(s, len > 0 ? qMin(100.0, 100.0 * left / len) : 0.0);
        left -= len;
    }
    return qMakePair(spines.last(), 100.0);
}

int spineIndexFromPosition(const QString &position)
{
    static const QRegularExpression re(QStringLiteral("epubcfi\\(/6/(\\d+)"));
    const auto m = re.match(position);
    if (!m.hasMatch())
        return -1;
    const int step = m.captured(1).toInt();
    if (step < 2 || step % 2 != 0)
        return -1;
    return step / 2 - 1;
}

namespace {

const Chapter *chapterById(const WorkDetails &d, qint64 id)
{
    for (const Chapter &c : d.chapters)
        if (c.id == id)
            return &c;
    return nullptr;
}

} // namespace

PositionLines describePositions(const ReaderPosition &reader, const ChapterMap &map, const WorkDetails &details,
                                bool canOpenThere)
{
    PositionLines lines;
    const Chapter *readerChapter = nullptr;
    if (reader.found) {
        if (reader.completed) {
            lines.reader = QStringLiteral("В читалке: прочитано");
        } else {
            // Matched only by order the chapter may be wrong (renamed and new chapters): pages only.
            if (map.exact)
                readerChapter = chapterById(details, map.chapterBySpine.value(spineIndexFromPosition(reader.position)));
            QStringList parts;
            if (readerChapter)
                parts << readerChapter->title;
            if (reader.pages > 0)
                parts << QStringLiteral("стр. %1 из %2").arg(reader.page).arg(reader.pages);
            if (!parts.isEmpty()) {
                lines.reader = QStringLiteral("В читалке: ") + parts.join(QStringLiteral(" · "));
            }
        }
    }
    const Chapter *siteChapter = details.lastChapterId ? chapterById(details, details.lastChapterId) : nullptr;
    if (siteChapter) {
        const int percent = qRound(details.lastChapterProgress);
        lines.site = QStringLiteral("На сайте: %1, %2").arg(siteChapter->title,
                                                         percent >= 100 ? QStringLiteral("прочитана") : QStringLiteral("%1 %").arg(percent));
    }
    if (readerChapter && siteChapter && siteChapter->sortOrder > readerChapter->sortOrder)
        lines.hint = canOpenThere ? QStringLiteral("На сайте вы дальше: «%1» — нажмите «Читать»").arg(siteChapter->title)
                                  : QStringLiteral("На сайте вы дальше — откройте «%1» через оглавление книги").arg(siteChapter->title);
    return lines;
}

namespace {

// Child elements of <body> (calibre puts the chapter's h1 and paragraphs there directly);
// -1 when the file cannot be read to the end of the body.
int bodyElementCount(const QByteArray &xhtml)
{
    int children = 0, depth = 0;
    bool inBody = false;
    QXmlStreamReader x(xhtml);
    while (!x.atEnd()) {
        const auto t = x.readNext();
        if (t == QXmlStreamReader::StartElement) {
            if (inBody && depth == 0)
                ++children;
            if (inBody)
                ++depth;
            else if (x.name() == QLatin1String("body"))
                inBody = true;
        } else if (t == QXmlStreamReader::EndElement && inBody) {
            if (depth == 0)
                return children; // </body>
            --depth;
        }
    }
    return -1; // a partly read body would count too few paragraphs
}

} // namespace

double chapterPercentFromPosition(const QString &position, const QByteArray &chapterXhtml)
{
    static const QRegularExpression re(QStringLiteral("!/4/(\\d+)"));
    const auto m = re.match(position);
    if (!m.hasMatch())
        return 0;
    const int element = m.captured(1).toInt() / 2; // 1-based among the body's child elements
    const int children = bodyElementCount(chapterXhtml);
    if (children <= 0 || element < 1)
        return 0;
    return qMin(100.0, 100.0 * (element - 1) / children);
}

int pageForPlace(const QList<int> &textLengths, int spineIndex, double chapterPercent,
                 int anchorSpine, double anchorPercent, int anchorPage, int pages)
{
    if (pages <= 0 || spineIndex < 0 || spineIndex >= textLengths.size())
        return 0;
    auto offset = [&textLengths](int spine, double percent) {
        double chars = 0;
        for (int i = 0; i < spine; ++i)
            chars += textLengths[i];
        return chars + textLengths[spine] * qBound(0.0, percent, 100.0) / 100.0;
    };
    double total = 0;
    for (int l : textLengths)
        total += l;
    if (total <= 0)
        return 0;
    const double target = offset(spineIndex, chapterPercent);
    double page;
    if (anchorSpine >= 0 && anchorSpine < textLengths.size() && anchorPage > 0 && anchorPage <= pages) {
        const double anchor = offset(anchorSpine, anchorPercent);
        if (target >= anchor)
            page = total > anchor ? anchorPage + (target - anchor) / (total - anchor) * (pages - anchorPage) : anchorPage;
        else
            page = 1 + target / anchor * (anchorPage - 1);
    } else {
        page = 1 + target / total * (pages - 1);
    }
    return qBound(1, int(page + 1e-6), pages); // down: a page early rather than skipping text
}

SiteOffer siteOffer(const WorkDetails &details, const ChapterMap &map, const ReaderPosition &reader,
                    const QList<int> &textLengths, const std::function<QByteArray(int)> &chapterFile)
{
    SiteOffer o;
    o.sitePercent = details.lastChapterProgress;
    o.siteTime = details.lastReadTime;
    auto no = [&o](const QString &why) {
        o.whyNot = why;
        return o;
    };
    if (!details.lastChapterId)
        return no(QStringLiteral("not read on the site"));
    if (!map.exact)
        return no(QStringLiteral("chapters matched only by order"));
    const int siteSpine = map.spineByChapter.value(details.lastChapterId, -1);
    const Chapter *site = chapterById(details, details.lastChapterId);
    if (siteSpine < 0 || !site)
        return no(QStringLiteral("the site's chapter is not in this file"));
    o.siteChapter = site->title;
    const QList<int> siteSpines = map.spinesByChapter.value(details.lastChapterId, {siteSpine});
    const int readerSpine = reader.found ? spineIndexFromPosition(reader.position) : -1;
    const qint64 readerChapterId = map.chapterBySpine.value(readerSpine);
    const double readerFilePercent = readerSpine >= 0 ? chapterPercentFromPosition(reader.position, chapterFile(readerSpine)) : 0;
    const double readerPercent = groupPercent(map.spinesByChapter.value(readerChapterId), textLengths, readerSpine, readerFilePercent);
    // A settings row without a place (a book never opened) has a time that means nothing.
    if (reader.found && readerSpine >= 0) {
        o.readerTime = reader.savedAt;
        const Chapter *rc = chapterById(details, readerChapterId);
        if (rc)
            o.readerChapter = rc->title;
        const bool sameChapter = readerChapterId == details.lastChapterId;
        if (sameChapter && qAbs(details.lastChapterProgress - readerPercent) < kSamePlacePercent)
            return no(QStringLiteral("the reader is already there"));
        o.byTime = details.lastReadTime.isValid() && reader.savedAt.isValid();
        if (o.byTime && details.lastReadTime <= reader.savedAt.addSecs(kClockSlackSecs))
            return no(QStringLiteral("the reader's place is newer (reader %1, site %2)")
                          .arg(reader.savedAt.toUTC().toString(Qt::ISODate), details.lastReadTime.toUTC().toString(Qt::ISODate)));
        if (!o.byTime) {
            const bool further = (rc ? site->sortOrder > rc->sortOrder : siteSpine > readerSpine)
                || (sameChapter && details.lastChapterProgress > readerPercent);
            if (!further)
                return no(QStringLiteral("the reader is further"));
        }
    }
    const QPair<int, double> target = placeInGroup(siteSpines, textLengths, details.lastChapterProgress);
    const bool anchored = reader.found && readerSpine >= 0;
    const int page = pageForPlace(textLengths, target.first, target.second, anchored ? readerSpine : -1,
                                  readerFilePercent, reader.found ? reader.page : 0, reader.found ? reader.pages : 0);
    if (page <= 0) {
        o.needsLayout = true;
        return no(QStringLiteral("the reader has no page count for this book yet"));
    }
    o.position = QString::number(page);
    return o;
}

QString whenText(const QDateTime &t, const QDateTime &now)
{
    static const char *const months[] = {"января", "февраля", "марта", "апреля", "мая", "июня",
                                         "июля", "августа", "сентября", "октября", "ноября", "декабря"};
    const QDateTime at = t.toTimeSpec(now.timeSpec());
    const QString hm = at.toString(QStringLiteral("HH:mm"));
    const qint64 days = at.date().daysTo(now.date());
    if (days == 0)
        return QStringLiteral("сегодня ") + hm;
    if (days == 1)
        return QStringLiteral("вчера ") + hm;
    QString day = QStringLiteral("%1 %2").arg(at.date().day()).arg(QString::fromUtf8(months[at.date().month() - 1]));
    if (at.date().year() != now.date().year())
        day += QStringLiteral(" %1").arg(at.date().year());
    return day + QLatin1Char(' ') + hm;
}
