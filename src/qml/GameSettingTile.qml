import QtQuick

Item {
    id: root
    property string label: ""
    property string detail: ""
    property string glyph: "settings"
    property color tint: Theme.blue
    property bool selected: false
    property bool compact: false
    signal activated()
    opacity: enabled ? 1 : .45
    Rectangle { x: 0; y: 5; width: parent.width; height: parent.height; radius: 20; color: "#30372e20" }
    Rectangle { anchors.fill: parent; radius: 20; border.color: Qt.darker(root.tint,1.3)
        gradient: Gradient { GradientStop { position: 0; color: Qt.lighter(root.tint,1.16) } GradientStop { position: .35; color: root.tint } GradientStop { position: 1; color: Qt.darker(root.tint,1.12) } }
        Rectangle { anchors.fill: parent; anchors.margins: 3; radius: 17; color: "transparent"; border.color: "#9ffffbe8" }
    }
    Rectangle { x: 15; y: root.compact ? (parent.height-height)/2 : 14; width: root.compact?32:40; height: width; radius: width/2; color: "#64fffbea"; border.color: "#70fffaf0"
        MenuGlyph { anchors.centerIn: parent; width: parent.width*.56; height: width; glyph: root.glyph }
    }
    Column { x: root.compact ? 59 : 15; y: root.compact ? (parent.height-height)/2 : 61; width: parent.width-x-(root.compact?32:17); spacing: 5
        Text { width: parent.width; text: root.label; textFormat: Text.PlainText; font.family: Theme.displayFamily; font.pixelSize: root.compact?18:20; color: Theme.ink; elide: Text.ElideRight }
        Text { width: parent.width; text: root.detail; textFormat: Text.PlainText; font.pixelSize: 12; color: Theme.ink; opacity: .8; elide: Text.ElideRight; visible: text.length>0 }
    }
    Text { visible: root.compact; anchors.right: parent.right; anchors.rightMargin: 15; anchors.verticalCenter: parent.verticalCenter; text: "›"; color: Theme.ink; font.pixelSize: 26 }
    Rectangle { anchors.fill: parent; anchors.margins: -2; radius: 22; color: "transparent"; border.width: root.selected?2:0; border.color: Theme.focusGlow }
    MouseArea { anchors.fill: parent; onClicked: root.activated() }
    Accessible.role: Accessible.Button; Accessible.name: label+" "+detail; Accessible.onPressAction: activated()
}
