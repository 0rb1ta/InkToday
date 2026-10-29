#include "Device.h"

#include "Log.h"

#include <common_utilities/einkinterface.h>
#include <inkview.h>

#include <QCoreApplication>
#include <QMetaObject>
#include <QSet>

bool device::isPocketbook()
{
    return true;
}

QString device::firmwareInfo()
{
    auto str = [](const char *s) { return s ? QString::fromUtf8(s) : QStringLiteral("?"); };
    return QStringLiteral("model=%1 hw=%2 fw=%3").arg(str(GetDeviceModel()), str(GetHardwareType()), str(GetSoftwareVersion()));
}

void device::openBook(const QString &path, int flags)
{
    const int rc = OpenBook(path.toUtf8().constData(), "", flags);
    Log::write(QStringLiteral("OpenBook(%1, flags=0x%2) -> %3").arg(path).arg(flags, 0, 16).arg(rc));
}

void device::openBookAt(const QString &path, const QString &parameters)
{
    const int rc = OpenBook(path.toUtf8().constData(), parameters.toUtf8().constData(), 0);
    Log::write(QStringLiteral("OpenBook(%1, \"%2\", 0) -> %3").arg(path, parameters).arg(rc));
}

void device::requireFullUpdate()
{
    Log::debug(QStringLiteral("full e-ink refresh"));
    if (auto *eink = pocketbook::utilities::einkPlatform())
        eink->requireFullUpdate();
}

bool device::networkConnected()
{
    return (QueryNetwork() & NET_CONNECTED) != 0;
}

namespace {
// NetConnectAsync takes a plain function pointer: the answer goes to the latest caller. One attempt
// at a time and the timeout are NetworkGate's job.
std::function<void(bool)> g_connectDone;

int onNetConnected(int status)
{
    Log::write(QStringLiteral("NetConnectAsync -> %1").arg(status));
    QMetaObject::invokeMethod(qApp, [status] {
        auto done = std::move(g_connectDone);
        g_connectDone = nullptr;
        if (done)
            done(status == NET_OK);
    }, Qt::QueuedConnection);
    return 0;
}
} // namespace

void device::connectNetworkAsync(std::function<void(bool)> done)
{
    g_connectDone = std::move(done);
    NetConnectAsync(onNetConnected);
}

void device::openNetworkDialog()
{
    const int rc = NetConnect(nullptr);
    Log::write(QStringLiteral("NetConnect(dialog) -> %1").arg(rc));
}

void device::installKeyHandler(std::function<void(int)> handler)
{
    auto *eink = pocketbook::utilities::einkPlatform();
    if (!eink) {
        Log::write(QStringLiteral("installKeyHandler: einkPlatform() is null"));
        return;
    }
    eink->addHandlerForInkvewEvents([handler](int type, int key, int) {
        if (type != EVT_KEYPRESS && type != EVT_KEYPRESS_EXT)
            return;
        int k;
        if (key == IV_KEY_NEXT || key == IV_KEY_NEXT2)
            k = KeyNext;
        else if (key == IV_KEY_PREV || key == IV_KEY_PREV2)
            k = KeyPrev;
        else if (key == IV_KEY_BACK)
            k = KeyBack;
        else
            return;
        QMetaObject::invokeMethod(qApp, [handler, k] { handler(k); }, Qt::QueuedConnection);
    });
}

void device::installForegroundHandler(std::function<void()> handler)
{
    auto *eink = pocketbook::utilities::einkPlatform();
    if (!eink)
        return;
    eink->addHandlerForInkvewEvents([handler](int type, int, int) {
        switch (type) {
        case EVT_KEYPRESS: case EVT_KEYRELEASE: case EVT_KEYPRESS_EXT: case EVT_KEYRELEASE_EXT:
        case EVT_POINTERUP: case EVT_POINTERDOWN: case EVT_POINTERMOVE: case EVT_REPAINT:
            return;
        default:
            break;
        }
        static QSet<int> seen;
        if (!seen.contains(type)) {
            seen.insert(type);
            QMetaObject::invokeMethod(qApp, [type] { Log::write(QStringLiteral("inkview event %1").arg(type)); }, Qt::QueuedConnection);
        }
        if (type == EVT_FOREGROUND || type == EVT_SHOW || type == EVT_ACTIVATE)
            QMetaObject::invokeMethod(qApp, [handler] { handler(); }, Qt::QueuedConnection);
    });
}

int device::screenDpi()
{
    return get_screen_dpi();
}

int device::panelHeight()
{
    return PanelHeight();
}
