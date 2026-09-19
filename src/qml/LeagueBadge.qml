import QtQuick

Item {
    id: root
    property string assetSource: ""
    property string badgeState: "unknown"
    readonly property bool imageReady: artwork.status === Image.Ready
    Rectangle {
        anchors.centerIn: parent; anchors.verticalCenterOffset: 2
        width: parent.width * 0.96; height: Math.min(parent.height * 0.9, width)
        radius: width * 0.25
        color: root.badgeState === "earned" ? "#778575" : "#b7c4ad"
        border.color: "#8e9e87"
    }
    Rectangle {
        anchors.centerIn: parent
        width: parent.width * 0.96; height: Math.min(parent.height * 0.9, width)
        radius: width * 0.25
        gradient: Gradient {
            GradientStop { position: 0; color: "#bbc9ad" }
            GradientStop { position: 1; color: "#eff4df" }
        }
        border.color: "#d8e3c8"
    }
    Image {
        id: artwork
        anchors.fill: parent
        source: root.assetSource
        sourceSize.width: 192; sourceSize.height: 192
        fillMode: Image.PreserveAspectFit
        mipmap: true
        cache: true; asynchronous: false
        opacity: root.badgeState === "earned" ? 1 : 0.18
    }
    Text {
        anchors.centerIn: parent
        visible: !root.imageReady
        text: root.badgeState === "unknown" ? "?" : "·"
        color: "#7b8a70"; font.pixelSize: Math.min(24, root.width * 0.5)
    }
}
