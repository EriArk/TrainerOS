import QtQuick

Item {
    id: root
    property string shape: "handheld"
    property color ink: "#34465c"
    Rectangle {
        anchors.centerIn: parent
        width: root.shape === "disc" ? 40 : 44; height: root.shape === "handheld" ? 52 : 38
        radius: root.shape === "disc" ? 20 : 6
        color: "transparent"; border.color: root.ink; border.width: 3
        rotation: root.shape === "cube" ? -12 : 0
        Rectangle {
            anchors.horizontalCenter: parent.horizontalCenter; y: 7
            width: root.shape === "disc" ? 12 : 26; height: root.shape === "disc" ? 12 : 17
            radius: root.shape === "disc" ? 6 : 2
            color: "transparent"; border.color: root.ink; border.width: 2
            anchors.verticalCenter: root.shape === "disc" ? parent.verticalCenter : undefined
        }
        Text { visible: root.shape === "handheld"; x: 7; y: 29; text: "+  ·"; color: root.ink; font.pixelSize: 19; font.bold: true }
    }
}
