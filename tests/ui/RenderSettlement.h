#pragma once
#include <QQuickItem>
#include <QQuickWindow>
// Resizes from a controller scenario reach the offscreen surface asynchronously.
// Retry a bounded number of ticks; a persistent layout error still reaches the
// ordinary screenshot/focus assertions and fails instead of being skipped.
inline bool waitForViewport(QQuickWindow* window,int& attempts) {
    if(auto* viewport=window->findChild<QQuickItem*>("viewport")) {
        const auto bounds=viewport->mapRectToScene(viewport->boundingRect());
        if(!QRectF(-1,-1,window->width()+2,window->height()+2).contains(bounds) && ++attempts<10) {
            window->requestUpdate();return true;
        }
    }
    attempts=0;return false;
}
inline bool waitForFocus(QQuickWindow* window,const QString& name,int& attempts) {
    const auto* item=window->activeFocusItem();
    if((!item || !item->isVisible() || item->objectName()!=name) && ++attempts<10) {
        window->requestUpdate();return true;
    }
    attempts=0;return false;
}
