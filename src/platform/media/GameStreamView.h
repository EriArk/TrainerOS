#pragma once
#include <QQuickItem>
#include <QVideoSink>
#include <QImage>
#include <qqmlregistration.h>

namespace trainer {
// LiveKit already decoded the frame. Qt's ordinary texture node supports both
// the handheld GPU scene graph and the software renderer used by other shells.
class GameStreamView : public QQuickItem {
    Q_OBJECT
    QML_ELEMENT
    Q_PROPERTY(QObject* videoSink READ videoSink CONSTANT)
public:
    explicit GameStreamView(QQuickItem* parent=nullptr);
    QObject* videoSink() {return &sink_;}
protected:
    QSGNode* updatePaintNode(QSGNode* previous,UpdatePaintNodeData*) override;
    void geometryChange(const QRectF& bounds,const QRectF& previous) override;
private:
    QVideoSink sink_{this};
    QImage image_;
    quint64 revision_=0;
};
}
