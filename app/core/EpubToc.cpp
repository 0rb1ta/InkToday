#include "EpubToc.h"

#include <QFileInfo>
#include <QHash>
#include <QXmlStreamReader>
#include <QtGui/private/qzipreader_p.h>

namespace {

QString attr(const QXmlStreamReader &x, const char *name)
{
    return x.attributes().value(QLatin1String(name)).toString();
}

// "OEBPS/content.opf" + "../Text/a.xhtml" -> "Text/a.xhtml"
QString resolve(const QString &baseDir, const QString &href)
{
    QString path = baseDir.isEmpty() ? href : baseDir + QLatin1Char('/') + href;
    QStringList parts;
    for (const QString &p : path.split(QLatin1Char('/'), QString::SkipEmptyParts)) {
        if (p == QLatin1String(".."))
            parts.isEmpty() ? void() : parts.removeLast();
        else if (p != QLatin1String("."))
            parts << p;
    }
    return parts.join(QLatin1Char('/'));
}

QString stripFragment(QString href)
{
    const int hash = href.indexOf(QLatin1Char('#'));
    return hash >= 0 ? href.left(hash) : href;
}

} // namespace

namespace {

struct Package {
    QStringList spinePaths; // file inside the zip for each spine item
    QString ncxPath;
};

Package readPackage(QZipReader &zip)
{
    Package pkg;

    // container.xml -> OPF path
    QString opfPath;
    QXmlStreamReader container(zip.fileData(QStringLiteral("META-INF/container.xml")));
    while (!container.atEnd() && opfPath.isEmpty()) {
        if (container.readNext() == QXmlStreamReader::StartElement && container.name() == QLatin1String("rootfile"))
            opfPath = attr(container, "full-path");
    }
    if (opfPath.isEmpty())
        return pkg;
    const QString opfDir = QFileInfo(opfPath).path() == QLatin1String(".") ? QString() : QFileInfo(opfPath).path();

    // OPF: manifest id -> path, spine order, toc file
    QHash<QString, QString> manifest;
    QStringList spine;
    QString tocId;
    QXmlStreamReader opf(zip.fileData(opfPath));
    while (!opf.atEnd()) {
        if (opf.readNext() != QXmlStreamReader::StartElement)
            continue;
        if (opf.name() == QLatin1String("item"))
            manifest.insert(attr(opf, "id"), resolve(opfDir, attr(opf, "href")));
        else if (opf.name() == QLatin1String("spine"))
            tocId = attr(opf, "toc");
        else if (opf.name() == QLatin1String("itemref"))
            spine << attr(opf, "idref");
    }
    QString ncxPath = manifest.value(tocId);
    if (ncxPath.isEmpty()) {
        for (const QString &p : manifest)
            if (p.endsWith(QLatin1String(".ncx")))
                ncxPath = p;
    }
    for (const QString &id : spine)
        pkg.spinePaths << manifest.value(id);
    pkg.ncxPath = ncxPath;
    return pkg;
}

} // namespace

QList<EpubNavPoint> readEpubToc(const QString &epubPath)
{
    if (!QFileInfo(epubPath).isFile())
        return {};
    QZipReader zip(epubPath);
    if (!zip.isReadable())
        return {};
    const Package pkg = readPackage(zip);
    const QString ncxPath = pkg.ncxPath;
    if (ncxPath.isEmpty() || pkg.spinePaths.isEmpty())
        return {};
    QHash<QString, int> spineIndexOf;
    for (int i = 0; i < pkg.spinePaths.size(); ++i)
        spineIndexOf.insert(pkg.spinePaths[i], i);

    // toc.ncx navPoints in document order
    QList<EpubNavPoint> toc;
    const QString ncxDir = QFileInfo(ncxPath).path() == QLatin1String(".") ? QString() : QFileInfo(ncxPath).path();
    QXmlStreamReader ncx(zip.fileData(ncxPath));
    QString title;
    bool inLabel = false;
    while (!ncx.atEnd()) {
        const auto t = ncx.readNext();
        if (t == QXmlStreamReader::StartElement) {
            if (ncx.name() == QLatin1String("navLabel"))
                inLabel = true;
            else if (inLabel && ncx.name() == QLatin1String("text"))
                title = ncx.readElementText().simplified();
            else if (ncx.name() == QLatin1String("content")) {
                const QString file = resolve(ncxDir, stripFragment(attr(ncx, "src")));
                if (spineIndexOf.contains(file))
                    toc << EpubNavPoint{spineIndexOf.value(file), title};
            }
        } else if (t == QXmlStreamReader::EndElement && ncx.name() == QLatin1String("navLabel")) {
            inLabel = false;
        }
    }
    return toc;
}

QByteArray readEpubSpineFile(const QString &epubPath, int spineIndex)
{
    if (!QFileInfo(epubPath).isFile())
        return {};
    QZipReader zip(epubPath);
    if (!zip.isReadable())
        return {};
    const Package pkg = readPackage(zip);
    if (spineIndex < 0 || spineIndex >= pkg.spinePaths.size())
        return {};
    return zip.fileData(pkg.spinePaths[spineIndex]);
}

QList<int> readEpubTextLengths(const QString &epubPath)
{
    QList<int> lengths;
    if (!QFileInfo(epubPath).isFile())
        return lengths;
    QZipReader zip(epubPath);
    if (!zip.isReadable())
        return lengths;
    const Package pkg = readPackage(zip);
    for (const QString &file : pkg.spinePaths) {
        int length = 0;
        bool inBody = false;
        QXmlStreamReader x(zip.fileData(file));
        while (!x.atEnd()) {
            const auto t = x.readNext();
            if (t == QXmlStreamReader::StartElement && x.name() == QLatin1String("body"))
                inBody = true;
            else if (t == QXmlStreamReader::Characters && inBody)
                length += x.text().toString().simplified().size();
        }
        lengths << length;
    }
    return lengths;
}

int readEpubSpineCount(const QString &epubPath)
{
    if (!QFileInfo(epubPath).isFile())
        return 0;
    QZipReader zip(epubPath);
    return zip.isReadable() ? readPackage(zip).spinePaths.size() : 0;
}
