import QtQuick

Item {
    id: root
    property string button: "A"
    property string label: "Select"
    property color tint: Theme.green
    property color labelColor: "#e2eee6"
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
            width: root.button.length > 1 ? 45 : 23; height: 23; radius: 7
            color: "#082f33"
            Rectangle {
                width: parent.width; height: 20; radius: 7
                gradient: Gradient {
                    GradientStop { position: 0; color: Qt.lighter(root.tint, 1.16) }
                    GradientStop { position: 1; color: root.tint }
                }
                border.color: Qt.darker(root.tint, 1.25)
                Text { anchors.centerIn: parent; text: root.button; color: Theme.ink; font.pixelSize: 11; font.bold: true }
            }
        }
        Text { text: root.label; color: root.labelColor; font.pixelSize: 12; anchors.verticalCenter: parent.verticalCenter }
    }
    MouseArea { anchors.fill: parent; anchors.topMargin: -5; anchors.bottomMargin: -5; enabled: root.interactive; cursorShape: Qt.PointingHandCursor; onClicked: root.clicked() }
}
