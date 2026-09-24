import QtQuick

Item {
    id: root
    property url picture: ""
    property url video: ""
    property bool playbackAllowed: false
    property bool settled: false
    readonly property bool eligible: visible && playbackAllowed && Qt.application.state === Qt.ApplicationActive && video.toString().startsWith("file:")
    function restart() { settled = false; delay.stop(); if (eligible) delay.start() }
    onVideoChanged: restart()
    onPictureChanged: restart()
    onEligibleChanged: restart()
    Component.onCompleted: restart()
    Timer { id: delay; interval: 700; onTriggered: root.settled = true }
    Image {
        id: still
        anchors.fill: parent
        source: root.visible ? root.picture : ""
        asynchronous: true; sourceSize.width: 720; sourceSize.height: 420
        fillMode: Image.PreserveAspectFit
    }
    SystemGlyph { anchors.centerIn: parent; width: 66; height: 70; shape: "handheld"; visible: still.status !== Image.Ready; opacity: 0.45 }
    Loader {
        id: videoLoader
        objectName: "game-preview-loader"
        anchors.fill: parent
        active: root.eligible && root.settled
        sourceComponent: GameVideoPreview { source: root.video }
    }
}
