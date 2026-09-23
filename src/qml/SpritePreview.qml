import QtQuick

Item {
    id: root
    property var asset: ({})
    property bool playing: false
    property int frame: 0
    property real pixelScale: 4
    readonly property int frames: Math.max(1, asset.frames || 1)
    readonly property bool ready: sheet.status === Image.Ready
    readonly property bool animating: visible && playing && ready && frames > 1 && !Theme.reducedMotion && Qt.application.state === Qt.ApplicationActive
    readonly property real cellWidth: sheet.sourceSize.width / frames
    readonly property real cellHeight: sheet.sourceSize.height
    readonly property real scaleFactor: cellWidth > 0 && cellHeight > 0 ? Math.min(pixelScale, width / cellWidth, height / cellHeight) : 1
    clip: true
    onAssetChanged: frame = 0
    onAnimatingChanged: if (!animating) frame = 0
    Item {
        objectName: "sprite-frame-viewport"
        anchors.centerIn: parent
        width: root.cellWidth * root.scaleFactor
        height: root.cellHeight * root.scaleFactor
        clip: true
        Image {
            id: sheet
            source: root.visible ? root.asset.url || "" : ""
            asynchronous: true; cache: false; smooth: false
            width: sourceSize.width * root.scaleFactor; height: sourceSize.height * root.scaleFactor
            x: -root.frame * root.cellWidth * root.scaleFactor
        }
    }
    Timer {
        running: root.animating; repeat: true
        interval: root.asset.durations && root.asset.durations.length > root.frame ? root.asset.durations[root.frame] : 120
        onTriggered: root.frame = (root.frame + 1) % root.frames
    }
}
