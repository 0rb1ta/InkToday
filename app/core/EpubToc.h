#pragma once

#include <QList>
#include <QString>

// One entry of an EPUB's table of contents and the position of its file in the reading order.
struct EpubNavPoint {
    int spineIndex = -1; // 0-based index in the OPF spine; epubcfi step /6/N has N = 2 * (index + 1)
    QString title;
};

// toc.ncx entries in their order, each mapped to the spine. Empty for anything that is not a
// readable EPUB (not a zip, no OPF, no toc.ncx).
QList<EpubNavPoint> readEpubToc(const QString &epubPath);

// The XHTML of the spine item at `spineIndex` (a chapter's text); empty when there is none.
QByteArray readEpubSpineFile(const QString &epubPath, int spineIndex);

// Visible text length of each spine item (characters, whitespace collapsed), in reading order;
// empty when the file is not a readable EPUB.
QList<int> readEpubTextLengths(const QString &epubPath);

// Number of files in the reading order (0 for anything that is not a readable EPUB).
int readEpubSpineCount(const QString &epubPath);
