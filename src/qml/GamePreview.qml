import QtQuick

Item {
    id: root
    property url picture: ""
    property url video: ""
    readonly property real imageAspect: still.status === Image.Ready && still.implicitHeight > 0 ? still.implicitWidth / still.implicitHeight : 1.5
    property bool themedFallback: false
    property string fallbackTitle: ""
    property url fallbackArt: ""
    property bool playbackAllowed: false
    property bool previewSound: false
    property int previewDelay: 1800
    property bool settled: false
    readonly property bool eligible: visible && playbackAllowed && Qt.application.state === Qt.ApplicationActive && video.toString().startsWith("file:")
    function restart() { settled = false; delay.stop(); if (eligible) delay.start() }
    onVideoChanged: restart()
    onPictureChanged: restart()
    onEligibleChanged: restart()
    Component.onCompleted: restart()
    Timer { id: delay; interval: Math.max(1500,Math.min(3000,root.previewDelay)); onTriggered: root.settled = true }
    Image {
        id: still
        anchors.fill: parent
        source: root.visible ? root.picture : ""
        asynchronous: true; sourceSize.width: 720; sourceSize.height: 420
        fillMode: Image.PreserveAspectFit
    }
    Loader {
        anchors.fill: parent
        active: root.visible && root.themedFallback && still.status !== Image.Ready
        sourceComponent: HomeCardBackdrop {
            radius: 4
            Rectangle { anchors.fill: parent; color: Theme.surfaceColor("#30203730"); radius: 4 }
            Image {
                id: fallbackImage; anchors.centerIn: parent
                width: parent.width * 0.78; height: parent.height * 0.72
                source: root.fallbackArt; asynchronous: true; fillMode: Image.PreserveAspectFit
                sourceSize: Qt.size(720, 400); mipmap: true
            }
            Text {
                anchors.centerIn: parent; width: parent.width * 0.8; height: parent.height * 0.7
                visible: fallbackImage.status !== Image.Ready
                text: root.fallbackTitle; textFormat: Text.PlainText
                font.family: Theme.gameFamily; font.pixelSize: 29; minimumPixelSize: 17; fontSizeMode: Text.Fit
                wrapMode: Text.WordWrap; maximumLineCount: 3; elide: Text.ElideRight
                horizontalAlignment: Text.AlignHCenter; verticalAlignment: Text.AlignVCenter
                color: Theme.textColor("#fff3cb"); style: Text.Outline; styleColor: Theme.chassisDark
            }
        }
    }
    SystemGlyph { anchors.centerIn: parent; width: 66; height: 70; shape: "handheld"; visible: !root.themedFallback && still.status !== Image.Ready; opacity: 0.45 }
    Loader {
        id: videoLoader
        objectName: "game-preview-loader"
        anchors.fill: parent
        active: root.eligible && root.settled
        sourceComponent: GameVideoPreview { source: root.video; sound: root.previewSound }
    }
}
