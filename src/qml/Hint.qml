import QtQuick

Row {
    id: root
    property string button: "A"
    property string label: "Select"
    property color tint: Theme.green
    property color labelColor: "#e2eee6"
    spacing: 7
    Rectangle {
        width: parent.button.length > 1 ? 45 : 23; height: 23; radius: 7
        color: "#082f33"
        Rectangle {
            width: parent.width; height: 20; radius: 7
            gradient: Gradient {
                GradientStop { position: 0; color: Qt.lighter(root.tint, 1.16) }
                GradientStop { position: 1; color: root.tint }
            }
            border.color: Qt.darker(parent.parent.tint, 1.25)
            Text { anchors.centerIn: parent; text: parent.parent.parent.button; color: Theme.ink; font.pixelSize: 11; font.bold: true }
        }
    }
    Text { text: parent.label; color: root.labelColor; font.pixelSize: 12; anchors.verticalCenter: parent.verticalCenter }
}
