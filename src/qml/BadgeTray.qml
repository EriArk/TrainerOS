import QtQuick

Rectangle {
    id: root
    property var slots: []
    property string count: "—"
    property string badgePrefix: "badge-"
    color: "#e3ebda"; border.color: "#b6c7b2"; radius: 13
    Rectangle { anchors.fill: parent; anchors.margins: 2; radius: 11; color: "transparent"; border.color: "#f9fcf4" }
    Text { x: 13; y: 8; text: "BADGES"; color: Theme.muted; font.pixelSize: 11; font.bold: true; font.letterSpacing: 1.5 }
    Text { x: 85; y: 2; width: parent.width - 99; horizontalAlignment: Text.AlignRight; text: root.count + (root.slots.length && root.count !== "—" ? " / " + root.slots.length : ""); color: Theme.ink; font.pixelSize: 23; font.bold: true }
    Row {
        x: 10; y: 33; spacing: 3
        Repeater {
            model: root.slots
            delegate: LeagueBadge {
                required property int index
                required property var modelData
                objectName: root.badgePrefix + index
                assetSource: modelData.image; badgeState: modelData.state
                width: (root.width - 20 - (root.slots.length - 1) * 3) / Math.max(1, root.slots.length)
                height: root.height - 42
            }
        }
    }
    LeagueBadge { x: 12; y: 38; width: 32; height: 42; visible: root.slots.length === 0 }
    Text {
        x: 52; y: 46; width: parent.width - 64
        visible: root.slots.length === 0
        text: root.count === "—" ? "No badge data yet" : "Badge identities unavailable"
        color: Theme.muted; font.pixelSize: 11; wrapMode: Text.WordWrap
    }
}
