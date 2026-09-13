import QtQuick

// Frame-attached trays/rails share a shallow molded lip. The caller owns the
// mount geometry and surface color; informational wells need no raised cap.
Rectangle {
    id: root
    color: "#d4e2d6"
    Rectangle {
        anchors.fill: parent; radius: root.radius
        gradient: Gradient {
            GradientStop { position: 0; color: Qt.lighter(root.color, 1.06) }
            GradientStop { position: 0.05; color: root.color }
            GradientStop { position: 1; color: Qt.darker(root.color, 1.035) }
        }
    }
    Rectangle { y: 0; width: parent.width; height: 1; color: Qt.darker(root.color, 1.3) }
    Rectangle { y: 1; width: parent.width; height: 2; color: Qt.lighter(root.color, 1.17) }
    Rectangle { anchors.bottom: parent.bottom; width: parent.width; height: 1; color: Qt.darker(root.color, 1.16) }
}
