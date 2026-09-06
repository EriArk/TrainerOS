import QtQuick

Row {
    property string button: "A"
    property string label: "Select"
    property color tint: Theme.green
    spacing: 7
    Rectangle {
        width: parent.button.length > 1 ? 45 : 23; height: 23; radius: 7
        color: "#082f33"
        Rectangle {
            width: parent.width; height: 20; radius: 7
            color: parent.parent.tint
            border.color: Qt.lighter(color, 1.3)
            Text { anchors.centerIn: parent; text: parent.parent.parent.button; color: Theme.ink; font.pixelSize: 11; font.bold: true }
        }
    }
    Text { text: parent.label; color: "#e2eee6"; font.pixelSize: 12; anchors.verticalCenter: parent.verticalCenter }
}
