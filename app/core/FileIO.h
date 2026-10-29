#pragma once

#include <QByteArray>
#include <QString>

// Saves `data` atomically, but only when it differs from what the file already holds: most state
// saves repeat the same bytes, and every rewrite wears the reader's flash. true: written or
// already equal; false: the write failed.
bool writeFileIfChanged(const QString &path, const QByteArray &data);
