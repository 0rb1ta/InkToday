#pragma once

#include <QFont>
#include <QColor>
#include <QIcon>
#include <QString>

// E-ink look shared by all screens: black / white / greys, 2 px borders, sizes in millimetres
// of the physical screen (buttons and rows at least 8 mm tall).
namespace Theme {

int mm(double millimetres);
QString fontFamily(); // chosen UI font family (empty: Qt default)
QString styleSheet();
QFont titleFont();
QFont bodyFont();
QFont smallFont();

// Font Awesome icon from resources: "house" (solid) or "regular:bookmark".
QIcon icon(const QString &name);
// The same icon filled with `color` (white icons on dark buttons).
QIcon icon(const QString &name, const QColor &color);

} // namespace Theme
