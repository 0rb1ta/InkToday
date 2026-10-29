#pragma once

#include <QString>

// Optional launcher icon in the reader's Applications menu. Firmware 6.x reads user apps from
// system/config/desktop/view.json: "applications" gets a "U_<launcher name>" entry with icon paths,
// and the key is listed in a "view" group. We only add or remove our own entry; the original file
// is copied next to it once (view.json.inktoday-backup) before the first change.
namespace DesktopIcon {

enum class Result { Installed, Removed, NoViewJson, BadJson, NoIcons, WriteFailed };

QString viewJsonPath(const QString &storageRoot = QStringLiteral("/mnt/ext1"));
bool isInstalled(const QString &viewJson);
Result install(const QString &viewJson, const QString &storageRoot = QStringLiteral("/mnt/ext1"));
Result remove(const QString &viewJson);
QString message(Result r);

} // namespace DesktopIcon
