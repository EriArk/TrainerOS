#include "GameStreamView.h"
#include <QQuickWindow>
#include <QSGSimpleTextureNode>
#include <QVideoFrame>

namespace trainer {
namespace {struct FrameNode final:QSGSimpleTextureNode {quint64 revision=0;};}
GameStreamView::GameStreamView(QQuickItem* parent):QQuickItem(parent) {
    setFlag(ItemHasContents);
    connect(&sink_,&QVideoSink::videoFrameChanged,this,[this](QVideoFrame frame){
        image_={};
        if(frame.pixelFormat()==QVideoFrameFormat::Format_BGRX8888&&frame.map(QVideoFrame::ReadOnly)) {
            image_=QImage(frame.bits(0),frame.width(),frame.height(),frame.bytesPerLine(0),QImage::Format_RGB32).copy();
            frame.unmap();
        }
        ++revision_;update();
    });
}
void GameStreamView::geometryChange(const QRectF& bounds,const QRectF& previous) {
    QQuickItem::geometryChange(bounds,previous);if(bounds.size()!=previous.size())update();
}
QSGNode* GameStreamView::updatePaintNode(QSGNode* previous,UpdatePaintNodeData*) {
    if(image_.isNull()){delete previous;return nullptr;}
    auto* node=static_cast<FrameNode*>(previous);if(!node)node=new FrameNode;
    if(node->revision!=revision_) {
        auto* texture=window()->createTextureFromImage(image_,QQuickWindow::TextureIsOpaque);
        if(!texture){delete node;return nullptr;}
        node->setTexture(texture);
        node->setOwnsTexture(true);node->setFiltering(QSGTexture::Linear);node->revision=revision_;
    }
    const auto size=QSizeF(image_.size()).scaled(QSizeF(width(),height()),Qt::KeepAspectRatio);
    node->setRect(QRectF(QPointF((width()-size.width())/2,(height()-size.height())/2),size));
    return node;
}
}
