import QtQuick

Rectangle {
    id: root
    property color surface: Theme.paper
    property bool patterned: true
    radius: 15
    color: Theme.chassisDark
    border.color: Theme.rim
    border.width: 1
    Rectangle {
        anchors.fill: parent
        anchors.margins: 2
        radius: 13
        gradient: Gradient {
            GradientStop { position: 0; color: Qt.darker(Theme.chassisDark, 1.12) }
            GradientStop { position: 0.5; color: Qt.lighter(Theme.chassis, 1.2) }
            GradientStop { position: 1; color: Qt.lighter(Theme.rim, 1.08) }
        }
    }
    Rectangle {
        anchors.fill: parent
        anchors.margins: 9
        radius: 7
        color: root.surface
        border.color: "#aabfb3"
        border.width: 1
        ShellBackgroundPattern { anchors.fill: parent; anchors.margins: 2; visible: root.patterned }
        Rectangle {
            anchors { left: parent.left; right: parent.right; bottom: parent.bottom; margins: 1 }
            height: 2; color: "#b3ffffff"
        }
        Rectangle {
            anchors { top: parent.top; left: parent.left; right: parent.right; margins: 1 }
            height: 6
            radius: 5
            gradient: Gradient {
                GradientStop { position: 0; color: "#243f3d38" }
                GradientStop { position: 1; color: "#003f3d38" }
            }
        }
    }
}
