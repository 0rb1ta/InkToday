#pragma once

#include "EpubToc.h"
#include "Models.h"
#include "ReaderLibrary.h"

#include <QHash>
#include <QPair>
#include <QString>

#include <functional>

// Which site chapter each file of a downloaded EPUB holds.
struct ChapterMap {
    QHash<int, qint64> chapterBySpine; // spine index -> site chapter id
    QHash<qint64, int> spineByChapter;              // the chapter's first file
    QHash<qint64, QList<int>> spinesByChapter;      // all its files: calibre splits long chapters
    bool exact = false; // titles matched; false: matched by order from the end ("примерно")
};

// Site chapters (by sortOrder) against the EPUB's table of contents. The books author.today
// serves carry the same chapter titles as the site; the entries before them (annotation, book
// title) are skipped by finding where the titles line up.
// spineCount (when known) lets the last chapter keep files split off after its start.
ChapterMap mapChapters(const QList<EpubNavPoint> &toc, const QList<Chapter> &siteChapters, int spineCount = -1);

// A chapter split over several files: percent of the whole chapter from a place in one file, and back.
double groupPercent(const QList<int> &spines, const QList<int> &textLengths, int spine, double filePercent);
QPair<int, double> placeInGroup(const QList<int> &spines, const QList<int> &textLengths, double groupPercent);

// Places this close in one chapter count as the same (pages and percents are estimates).
constexpr double kSamePlacePercent = 5;

// Spine index from a reader position like "pbr:/webkit?##epubcfi(/6/10!/4/2/5:9)" (step N of
// /6/N is 2 * (index + 1)); -1 when the position is not an EPUB CFI.
int spineIndexFromPosition(const QString &position);

// Lines for the book card. Empty strings when there is nothing to say.
struct PositionLines {
    QString reader; // "В читалке: Глава 2 · стр. 37 из 257"
    QString site;   // "На сайте: Глава 12, 56 %"
    QString hint;   // when the site is further than the reader
};
// canOpenThere: «Читать» offers the site's place (setting), so the hint points there.
PositionLines describePositions(const ReaderPosition &reader, const ChapterMap &map, const WorkDetails &details,
                                bool canOpenThere = false);

// Percent of a chapter from a reader position "...epubcfi(/6/N!/4/K...)": element K/2 among the
// children of <body> of that chapter's XHTML. 0 when it cannot be told.
double chapterPercentFromPosition(const QString &position, const QByteArray &chapterXhtml);

// Which of two places is newer: the site's time and the reader's clock differ by a few seconds
// (measured on the device), so closer than this counts as "the same time".
constexpr int kClockSlackSecs = 15;

// The reader page for a place in the book: `chapterPercent` into spine item `spineIndex`. The
// firmware opens a book only at a page number, so the place is turned into one by characters:
// between the reader's own known page (`anchorSpine`/`anchorPercent` on `anchorPage`, skipped when
// anchorSpine < 0) and the book's ends. About a page off, rounded down. 0 when it cannot be told.
int pageForPlace(const QList<int> &textLengths, int spineIndex, double chapterPercent,
                 int anchorSpine, double anchorPercent, int anchorPage, int pages);

// Whether "Читать" should offer the place last read on the site. The newer place wins (the site's
// lastReadTime against the time the reader saved its position, kClockSlackSecs apart at least);
// without the site's time, the further place wins. Only with chapters matched by title.
struct SiteOffer {
    QString position;      // the page to open at; empty: open as usual (whyNot says why)
    bool needsLayout = false; // the site is newer, but the reader has no page count for this book yet
    QString siteChapter;   // titles for the question
    double sitePercent = 0;
    QString readerChapter;
    QDateTime siteTime, readerTime;
    bool byTime = false;   // false: decided by order, the site's time was unknown
    QString whyNot;
};
SiteOffer siteOffer(const WorkDetails &details, const ChapterMap &map, const ReaderPosition &reader,
                    const QList<int> &textLengths, const std::function<QByteArray(int spineIndex)> &chapterFile);

// "сегодня 14:20", "вчера 23:05", "21 октября 14:20" (local time).
QString whenText(const QDateTime &t, const QDateTime &now);
