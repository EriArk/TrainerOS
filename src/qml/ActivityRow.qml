import QtQuick

Item {
    id: root
    property string label
    property string detail
    property string glyph: "chevron"
    property color accent: Theme.blue
    property bool selected: false
    signal activated()
    height: detail.length ? 59 : 44
    opacity: enabled ? 1 : .45
    Rectangle { anchors.fill: parent; anchors.margins: 1; radius: 10; color: root.selected ? "#18edbd50" : "transparent"; border.width: root.selected ? 2 : 0; border.color: Theme.focusGlow }
    Rectangle { x: 7; anchors.verticalCenter: parent.verticalCenter; width: 30; height: 30; radius: 15; color: root.accent }
    MenuGlyph { x: 14; anchors.verticalCenter: parent.verticalCenter; width: 16; height: 16; glyph: root.glyph }
    Column { x: 47; anchors.verticalCenter: parent.verticalCenter; width: parent.width-x-23; spacing: 4
        Text { width: parent.width; text: root.label; color: Theme.ink; font.family: Theme.displayFamily; font.pixelSize: 18; elide: Text.ElideRight; textFormat: Text.PlainText }
        Text { width: parent.width; visible: text.length; text: root.detail; color: Theme.muted; font.pixelSize: 12; elide: Text.ElideRight; textFormat: Text.PlainText }
    }
    Text { anchors.right: parent.right; anchors.rightMargin: 7; anchors.verticalCenter: parent.verticalCenter; text: "›"; color: Theme.muted; font.pixelSize: 22 }
    Rectangle { x: 47; width: parent.width-x-9; height: 1; anchors.bottom: parent.bottom; color: "#23476a61" }
    MouseArea { anchors.fill: parent; onClicked: root.activated() }
    Accessible.role: Accessible.Button; Accessible.name: label; Accessible.onPressAction: activated()
}
