import QtQuick

Image {
    property int art: 0
    source: "qrc:/crates/illustrations-wide.png"
    sourceSize: Qt.size(1254, 1254)
    sourceClipRect: Qt.rect(0, art * 209, 1254, 209)
    fillMode: Image.PreserveAspectCrop
    asynchronous: true
    smooth: true
}
