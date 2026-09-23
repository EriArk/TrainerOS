import QtQuick

Item {
    id: root
    property var media: ({})
    readonly property bool available: cover.status === Image.Ready || logo.status === Image.Ready
    Image {
        id: logo
        width: parent.width; height: 52
        source: root.media.marquee || ""
        sourceSize: Qt.size(800, 160)
        fillMode: Image.PreserveAspectFit; asynchronous: true
    }
    Image {
        id: cover
        y: logo.status === Image.Ready ? 62 : 0
        width: parent.width; height: parent.height - y
        source: root.media.cover || ""
        sourceSize: Qt.size(800, 600)
        fillMode: Image.PreserveAspectFit; asynchronous: true
    }
}
