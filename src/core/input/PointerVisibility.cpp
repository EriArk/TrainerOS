#include "PointerVisibility.h"
#include <QCursor>
#include <QMouseEvent>
#include <QPointingDevice>

namespace trainer {
PointerVisibility::PointerVisibility(QWindow& window, bool dedicatedSession)
    : QObject(&window), window_(window), dedicatedSession_(dedicatedSession) {
    window_.installEventFilter(this);
    hide();
}
void PointerVisibility::hide() {
    hidden_ = true;
    window_.setCursor(Qt::BlankCursor);
}
bool PointerVisibility::eventFilter(QObject*, QEvent* event) {
    switch (event->type()) {
    case QEvent::CursorChange:
        // Qt Quick updates the window cursor after delivering pointer events.
        // Keep the policy when a MouseArea restores its default arrow.
        if (hidden_ && window_.cursor().shape() != Qt::BlankCursor) hide();
        break;
    case QEvent::TouchBegin: case QEvent::TouchUpdate: case QEvent::TouchEnd:
    case QEvent::TouchCancel:
        hide(); break;
    case QEvent::MouseMove: case QEvent::MouseButtonPress:
    case QEvent::MouseButtonRelease: case QEvent::MouseButtonDblClick: {
        const auto* mouse = static_cast<QMouseEvent*>(event);
        // Gamescope's default touch mode presents taps as ordinary X11 mouse
        // events. The dedicated handheld shell therefore keeps its cursor blank.
        // In a desktop window, genuine mouse use can still restore the pointer.
        if (dedicatedSession_ || mouse->source() != Qt::MouseEventNotSynthesized
            || mouse->pointingDevice()->type() == QInputDevice::DeviceType::TouchScreen)
            hide();
        else { hidden_ = false; window_.unsetCursor(); }
        break;
    }
    default: break;
    }
    return false;
}
}
