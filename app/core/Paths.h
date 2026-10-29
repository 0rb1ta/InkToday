#pragma once

#include <QString>

// Where the app keeps its own data and where downloaded books go.
// App data lives under <storage>/system so the reader's library never indexes it.
namespace Paths {

QString dataDir(const QString &storageRoot = QStringLiteral("/mnt/ext1"));
QString booksDir(const QString &storageRoot = QStringLiteral("/mnt/ext1"));

} // namespace Paths
