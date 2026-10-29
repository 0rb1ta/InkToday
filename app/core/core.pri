# Platform-independent core: built for the device and for host unit tests.
QT += network gui gui-private sql
INCLUDEPATH += $$PWD $$PWD/../third_party/qrcodegen
SOURCES += \
    $$PWD/Paths.cpp $$PWD/FileNaming.cpp $$PWD/Log.cpp \
    $$PWD/ApiError.cpp $$PWD/RateLimiter.cpp \
    $$PWD/QtHttpTransport.cpp $$PWD/ApiClient.cpp \
    $$PWD/Session.cpp $$PWD/AuthService.cpp \
    $$PWD/Models.cpp $$PWD/LibraryStore.cpp $$PWD/LibrarySync.cpp \
    $$PWD/DownloadIndex.cpp $$PWD/Downloader.cpp \
    $$PWD/StartupFlow.cpp $$PWD/BookStatus.cpp $$PWD/StatusCache.cpp \
    $$PWD/Settings.cpp $$PWD/HomeFeed.cpp $$PWD/LibraryActions.cpp $$PWD/CoverCache.cpp \
    $$PWD/Qr.cpp $$PWD/PageMath.cpp $$PWD/DesktopIcon.cpp $$PWD/UpdateCheck.cpp $$PWD/Catalog.cpp $$PWD/SiteSearch.cpp $$PWD/WorkList.cpp $$PWD/FileIO.cpp $$PWD/EpubToc.cpp $$PWD/ReadingPosition.cpp $$PWD/ReaderLibrary.cpp $$PWD/PositionUpload.cpp $$PWD/NetworkGate.cpp $$PWD/../third_party/qrcodegen/qrcodegen.cpp
HEADERS += \
    $$PWD/Paths.h $$PWD/FileNaming.h $$PWD/Log.h \
    $$PWD/ApiError.h $$PWD/RateLimiter.h \
    $$PWD/HttpTransport.h $$PWD/QtHttpTransport.h $$PWD/ApiClient.h \
    $$PWD/Session.h $$PWD/AuthService.h \
    $$PWD/Models.h $$PWD/LibraryStore.h $$PWD/LibrarySync.h \
    $$PWD/DownloadIndex.h $$PWD/Downloader.h \
    $$PWD/StartupFlow.h $$PWD/BookStatus.h $$PWD/StatusCache.h \
    $$PWD/Settings.h $$PWD/HomeFeed.h $$PWD/LibraryActions.h $$PWD/CoverCache.h $$PWD/Qr.h $$PWD/PageMath.h $$PWD/DesktopIcon.h $$PWD/UpdateCheck.h $$PWD/Catalog.h $$PWD/SiteSearch.h $$PWD/WorkList.h $$PWD/FileIO.h $$PWD/EpubToc.h $$PWD/ReadingPosition.h $$PWD/ReaderLibrary.h $$PWD/PositionUpload.h $$PWD/NetworkGate.h
