#include "globalhotkey.h"

#include <QCoreApplication>

#ifdef Q_OS_WIN
#include <windows.h>
#endif

namespace {

constexpr int kHotkeyId = 0x534E;

}  // namespace

GlobalHotkey::GlobalHotkey(QObject *parent)
    : QObject(parent)
{
}

GlobalHotkey::~GlobalHotkey()
{
    unregisterShortcut();
}

bool GlobalHotkey::registerShortcut()
{
    if (m_registered)
        return true;

#ifdef Q_OS_WIN
    m_registered = RegisterHotKey(
        nullptr, kHotkeyId,
        MOD_CONTROL | MOD_ALT | MOD_NOREPEAT,
        static_cast<UINT>('A')) != FALSE;

    if (m_registered && QCoreApplication::instance())
        QCoreApplication::instance()->installNativeEventFilter(this);
#endif

    return m_registered;
}

void GlobalHotkey::unregisterShortcut()
{
    if (!m_registered)
        return;

#ifdef Q_OS_WIN
    UnregisterHotKey(nullptr, kHotkeyId);
    if (QCoreApplication::instance())
        QCoreApplication::instance()->removeNativeEventFilter(this);
#endif

    m_registered = false;
}

#if QT_VERSION >= QT_VERSION_CHECK(6, 0, 0)
bool GlobalHotkey::nativeEventFilter(const QByteArray &eventType, void *message,
                                     qintptr *result)
#else
bool GlobalHotkey::nativeEventFilter(const QByteArray &eventType, void *message,
                                     long *result)
#endif
{
    Q_UNUSED(eventType)
    Q_UNUSED(result)

#ifdef Q_OS_WIN
    auto *msg = static_cast<MSG *>(message);
    if (msg && msg->message == WM_HOTKEY
        && static_cast<int>(msg->wParam) == kHotkeyId) {
        emit activated();
        return true;
    }
#endif

    return false;
}
