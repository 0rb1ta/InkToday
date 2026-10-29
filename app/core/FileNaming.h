#pragma once

#include <QString>

// "<author> - <title>.<ext>", safe for FAT32 (the reader's storage) and at most 240 UTF-8 bytes.
QString bookFileName(const QString &author, const QString &title, const QString &ext);
