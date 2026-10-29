#pragma once

#include <QMetaType>
#include <QString>

struct DownloadEntry;
struct WorkDetails;

// What the single action button of a book shows.
// Subscribers: the author opened downloads to subscribers only (privacyDownloads OnlySubscribers).
enum class ActionState { Read, Download, Update, Buy, SiteOnly, Subscribers, Unknown };

struct BookFacts {
    bool loggedIn = false;
    bool haveDetails = false;
    bool allowDownloads = false;
    bool subscribersOnly = false; // downloads for the author's subscribers (not this reader)
    bool isPurchased = false;
    double price = 0;
    bool downloaded = false;
    bool fileMissing = false;
    bool siteNewer = false;
};
Q_DECLARE_METATYPE(BookFacts)

ActionState resolveAction(const BookFacts &facts);
QString actionText(ActionState state);
// "+N гл." / "обновлено" on a downloaded book that the site has updated; empty when there is
// nothing the reader could do about it (file gone, or the author closed downloads).
QString updateBadge(const BookFacts &facts, int newChapters);
// Chapters added on the site since the book was downloaded.
int newChapters(const DownloadEntry &entry, const WorkDetails &details);
