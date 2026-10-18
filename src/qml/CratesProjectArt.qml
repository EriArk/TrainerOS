import QtQuick

Item {
    id: root
    property string source: ""
    property string kind: "game"
    property bool crop: true
    property color accent: Theme.rim
    clip: true
    Rectangle { anchors.fill: parent; color: Qt.tint(Theme.paper,Qt.alpha(root.accent,.19)) }
    Item {
        anchors.centerIn: parent; width: Math.min(parent.width,parent.height)*.65; height: width
        visible: picture.status !== Image.Ready; opacity: .5
        Rectangle { anchors.fill: parent; radius: width/2; color: "transparent"; border.color: root.accent }
        Rectangle { anchors.fill: parent; anchors.margins: 8; radius: width/2; color: "transparent"; border.color: root.accent }
        Image { anchors.centerIn: parent; width: parent.width*.4; height: width; fillMode: Image.PreserveAspectFit
            source: root.kind === "module" ? "qrc:/social-icons/gear.svg" : root.kind === "app" ? "qrc:/social-icons/squares-four.svg" : "qrc:/crates/gamepad.svg" }
    }
    Image {
        id: picture; anchors.fill: parent; source: root.source
        asynchronous: true; sourceSize: Qt.size(900,650)
        fillMode: root.crop ? Image.PreserveAspectCrop : Image.PreserveAspectFit
    }
}
