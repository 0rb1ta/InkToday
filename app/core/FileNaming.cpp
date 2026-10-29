#include "FileNaming.h"

namespace {

const int kMaxBytes = 240;
// FAT32 allows 255 UTF-16 units; " (<id>)" and ".part" are added to the name later.
const int kMaxUnits = 255 - 22 - 5;

QString cleanPart(const QString &raw)
{
    QString s;
    s.reserve(raw.size());
    for (QChar c : raw) {
        if (c.unicode() < 0x20 || QStringLiteral("/\\:*?\"<>|").contains(c))
            s.append(QLatin1Char('_'));
        else
            s.append(c);
    }
    s = s.simplified();
    // FAT32 drops trailing dots/spaces; strip them (and leading ones) so names stay stable.
    while (!s.isEmpty() && (s.endsWith(QLatin1Char('.')) || s.endsWith(QLatin1Char(' '))))
        s.chop(1);
    while (!s.isEmpty() && (s.startsWith(QLatin1Char('.')) || s.startsWith(QLatin1Char(' '))))
        s.remove(0, 1);
    return s;
}

} // namespace

QString bookFileName(const QString &author, const QString &title, const QString &ext)
{
    const QString a = cleanPart(author);
    QString t = cleanPart(title);
    const QString suffix = QLatin1Char('.') + ext;

    auto compose = [&](const QString &titlePart) {
        if (a.isEmpty() && titlePart.isEmpty())
            return QStringLiteral("book") + suffix;
        if (a.isEmpty() || titlePart.isEmpty())
            return (a.isEmpty() ? titlePart : a) + suffix;
        return a + QStringLiteral(" - ") + titlePart + suffix;
    };

    QString name = compose(t);
    // Chop whole characters: a lone half of a surrogate pair (emoji) is not valid text.
    auto chopChar = [](QString &s) {
        s.chop(1);
        if (!s.isEmpty() && s.at(s.size() - 1).isHighSurrogate())
            s.chop(1);
    };
    while ((name.toUtf8().size() > kMaxBytes || name.size() > kMaxUnits) && !t.isEmpty()) {
        chopChar(t);
        t = cleanPart(t);
        name = compose(t);
    }
    if (name.toUtf8().size() > kMaxBytes || name.size() > kMaxUnits) {
        // Author alone is too long: keep as many characters as fit.
        QString cut = a;
        while (!cut.isEmpty() && ((cut + suffix).toUtf8().size() > kMaxBytes || (cut + suffix).size() > kMaxUnits))
            chopChar(cut);
        name = cleanPart(cut) + suffix;
    }
    return name;
}
