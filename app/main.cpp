// InkToday: unofficial author.today client for PocketBook.

#include "AppController.h"
#include "Device.h"
#include "Log.h"
#include "Paths.h"

#include <QApplication>
#include <QDateTime>
#include <QFileInfo>
#include <QTextCodec>

namespace {

// Firmware 6.8 passes QT_QPA_PLATFORM=pocketbook, but only
// the pocketbook2 plugin exists.
void prepareQtEnvironment()
{
    if (QFileInfo::exists(QStringLiteral("/ebrmain/lib/qtplugins/platforms/libqpocketbook2.so"))) {
        qputenv("QT_QPA_PLATFORM", "pocketbook2");
        qputenv("QT_QPA_PLATFORM_PLUGIN_PATH", "/ebrmain/lib/qtplugins/platforms");
        qputenv("QT_PLUGIN_PATH", "/ebrmain/lib/qtplugins");
    }
}

// Book file names are Cyrillic. If the firmware starts us with a non-UTF-8 locale, Qt 5.12 would
// encode them through that codec and FAT32 rejects the result ('?' replacements).
QString ensureUtf8FileNames()
{
    const QByteArray before = QTextCodec::codecForLocale()->name();
    if (before != "UTF-8")
        QTextCodec::setCodecForLocale(QTextCodec::codecForName("UTF-8"));
    return QStringLiteral("locale codec %1 -> %2, LANG=%3 LC_ALL=%4")
        .arg(QString::fromLatin1(before), QString::fromLatin1(QTextCodec::codecForLocale()->name()),
             QString::fromLocal8Bit(qgetenv("LANG")), QString::fromLocal8Bit(qgetenv("LC_ALL")));
}

} // namespace

int main(int argc, char **argv)
{
    prepareQtEnvironment();
    QApplication app(argc, argv);
    const QString codecInfo = ensureUtf8FileNames();

    const QString data = Paths::dataDir();
    Log::init(data + QStringLiteral("/log.txt"));
    Log::write(QStringLiteral("==== InkToday %1 start, %2, platform %3")
                   .arg(QStringLiteral(APP_VERSION), device::firmwareInfo(), QApplication::platformName()));
    Log::write(codecInfo);

    AppController window;
    window.showFullScreen();
    window.start();
    device::requireFullUpdate();

    const int rc = app.exec();
    Log::write(QStringLiteral("==== exit %1").arg(rc));
    return rc;
}
