import QtQuick

Item {
    id: root
    required property var entry
    property bool selected: false
    property color accent: Theme.rim
    readonly property var release: (entry.releases || [])[0] || ({})
    signal activated()
    Rectangle { x: 1; y: 4; width: parent.width; height: parent.height; radius: 12; color: "#25263830" }
    Rectangle {
        anchors.fill: parent; radius: 11; border.color: Qt.alpha(root.accent,.65)
        gradient: Gradient { GradientStop { position: 0; color: Theme.paper } GradientStop { position: 1; color: "#f2eddf" } }
    }
    CratesProjectArt {
        x: 5; y: 5; width: parent.width-10; height: parent.height-51
        source: root.entry.hero || (root.entry.screenshots || [])[0] || ""
        kind: root.entry.kind; accent: root.accent
    }
    Image {
        x: 12; y: parent.height-37; width: 19; height: 19; fillMode: Image.PreserveAspectFit
        source: root.entry.kind === "module" ? "qrc:/social-icons/gear.svg" : root.entry.kind === "app" ? "qrc:/social-icons/squares-four.svg" : "qrc:/crates/gamepad.svg"
    }
    Text {
        x: 38; y: parent.height-42; width: parent.width-x-12; height: 23
        text: root.entry.title; textFormat: Text.PlainText; elide: Text.ElideRight
        font.family: Theme.displayFamily; font.pixelSize: 18; color: Theme.ink
    }
    Text {
        x: 38; y: parent.height-21; width: parent.width-x-12
        text: root.entry.sample ? "Example · "+root.entry.license : root.release.version ? "v"+root.release.version+" · "+(root.release.compatibility === "supported" ? "Supported" : root.release.compatibility === "incompatible" ? "Not compatible" : "Not tested") : root.entry.creator || ""
        textFormat: Text.PlainText; elide: Text.ElideRight; font.pixelSize: 11; color: Theme.muted
    }
    Rectangle { anchors.fill: parent; anchors.margins: -3; radius: 14; color: "transparent"; border.width: 3; border.color: "#55ffbd24"; visible: root.selected }
    Rectangle { anchors.fill: parent; anchors.margins: -1; radius: 12; color: "transparent"; border.width: 2; border.color: Theme.focus; visible: root.selected }
    MouseArea { anchors.fill: parent; onClicked: root.activated(); cursorShape: Qt.PointingHandCursor }
    Accessible.role: Accessible.Button
    Accessible.name: entry.title
    Accessible.onPressAction: root.activated()
}
