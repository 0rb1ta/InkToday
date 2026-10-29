# InkToday: unofficial author.today client for PocketBook.
# Device build: app/package/build-device.sh. Host build (stub device layer) works with the SDK's x86 Qt.
QT += widgets network svg
TARGET = inktoday

include(common.pri)
include(core/core.pri)
include(device/device.pri)

INCLUDEPATH += $$PWD/ui $$PWD/ui/kit $$PWD/ui/screens
SOURCES += main.cpp \
    ui/AppContext.cpp ui/AppController.cpp ui/BookActions.cpp \
    ui/kit/Theme.cpp ui/kit/Widgets.cpp \
    ui/screens/StartScreen.cpp ui/screens/HomeScreens.cpp ui/screens/BookScreens.cpp ui/screens/MyScreens.cpp \
    ui/screens/WorkGrid.cpp ui/screens/CatalogScreens.cpp ui/screens/AuthorScreens.cpp
HEADERS += \
    ui/AppContext.h ui/AppController.h ui/BookActions.h \
    ui/kit/Theme.h ui/kit/Widgets.h \
    ui/screens/StartScreen.h ui/screens/HomeScreens.h ui/screens/BookScreens.h ui/screens/MyScreens.h \
    ui/screens/WorkGrid.h ui/screens/CatalogScreens.h ui/screens/AuthorScreens.h
RESOURCES += resources.qrc
