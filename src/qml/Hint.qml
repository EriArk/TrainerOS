import QtQuick

Item {
    id: root
    property string button: "A"
    readonly property string glyph: { const family=Theme.controller?.family; const profile=Theme.controller?.profile; return Theme.buttonLabel(button) }
    property string label: "Select"
    property color tint: Theme.green
    property bool subtle: false
    property color labelColor: subtle ? "#b7c8c1" : "#e2eee6"
    property bool interactive: false
    signal clicked()
    implicitWidth: content.width
    implicitHeight: content.height
    Accessible.role: interactive ? Accessible.Button : Accessible.StaticText
    Accessible.name: label
    Accessible.onPressAction: if (interactive) clicked()
    Row {
        id: content
        spacing: 7
        Rectangle {
            width: Math.max(23,root.glyph.length*7+10); height: 23; radius: 7
            color: root.subtle ? "transparent" : "#082f33"
            Rectangle {
                width: parent.width; height: 20; radius: 7
                gradient: Gradient {
                    GradientStop { position: 0; color: root.subtle ? Qt.alpha(root.labelColor, 0.06) : Qt.lighter(root.tint, 1.16) }
                    GradientStop { position: 1; color: root.subtle ? Qt.alpha(root.labelColor, 0.06) : root.tint }
                }
                border.color: root.subtle ? Qt.alpha(root.labelColor, 0.32) : Qt.darker(root.tint, 1.25)
                Text { anchors.centerIn: parent; text: root.glyph; color: root.subtle ? Qt.tint(root.labelColor, Qt.alpha(root.tint, 0.3)) : Theme.ink; font.pixelSize: 11; font.bold: true }
            }
        }
        Text { text: root.label; color: root.labelColor; font.pixelSize: 12; anchors.verticalCenter: parent.verticalCenter }
    }
    MouseArea { anchors.fill: parent; anchors.topMargin: -5; anchors.bottomMargin: -5; enabled: root.interactive; cursorShape: Qt.PointingHandCursor; onClicked: root.clicked() }
}
