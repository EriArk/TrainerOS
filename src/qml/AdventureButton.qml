import QtQuick

Item {
    id: root
    required property var shell
    objectName: "home-launch"
    property bool selected: shell.page === 0 && shell.focusIndex === 0 && !shell.drawerOpen && !shell.menuOpen && !shell.serviceOpen && shell.notice.length === 0
    property bool depressed: false
    onSelectedChanged: if (selected && visible) forceActiveFocus(Qt.OtherFocusReason)
    onVisibleChanged: if (selected && visible) forceActiveFocus(Qt.OtherFocusReason)
    Component.onCompleted: if (selected && visible) forceActiveFocus(Qt.OtherFocusReason)
    Connections { target: root.shell; function onHomeLaunchPressed() { root.depressed = true; release.restart() } }
    Timer { id: release; interval: 150; onTriggered: root.depressed = false }
    Rectangle {
        anchors.centerIn: parent; width: 198; height: 198; radius: 99
        color: Theme.chassis; border.color: Theme.rim; border.width: 3
        Rectangle { anchors.fill: parent; anchors.margins: 7; radius: width / 2; color: Theme.chassisDark; border.color: Qt.darker(Theme.chassisDark, 1.3); border.width: 3 }
    }
    Rectangle {
        anchors.centerIn: parent; width: 214; height: 214; radius: 107
        color: "transparent"; border.width: root.selected ? 3 : 0; border.color: Theme.focus
    }
    Rectangle {
        x: (parent.width - 170) / 2; y: (parent.height - 170) / 2 + 8
        width: 170; height: 170; radius: 85; color: "#693b2e"
    }
    Item {
        x: (parent.width - 170) / 2; y: (parent.height - 170) / 2 + (root.depressed ? 5 : -2)
        width: 170; height: 170
        Behavior on y { NumberAnimation { duration: Theme.motion(75) } }
        Rectangle {
            anchors.fill: parent; radius: 85; border.color: "#85442e"; border.width: 2
            gradient: Gradient {
                GradientStop { position: 0; color: "#ffdb95" }
                GradientStop { position: 0.45; color: "#f5ad68" }
                GradientStop { position: 1; color: "#cf7549" }
            }
        }
        Rectangle {
            anchors.fill: parent; anchors.margins: 7; radius: width / 2
            color: "transparent"; border.color: "#ffe8b6"; border.width: 2
        }
        Rectangle {
            x: 35; y: 32; width: 100; height: 100; radius: 50
            color: "#ec9b58"; border.color: "#b3683e"; border.width: 2
            Text { anchors.centerIn: parent; anchors.verticalCenterOffset: 2; text: "A"; color: "#ffdfa2"; font.pixelSize: 63; font.bold: true }
            Text { anchors.centerIn: parent; text: "A"; color: "#70452f"; font.pixelSize: 63; font.bold: true }
        }
        Rectangle { x: 55; y: 16; width: 37; height: 2; rotation: -8; color: "#fff0c4"; opacity: 0.7 }
    }
    MouseArea { anchors.fill: parent; onClicked: root.shell.activate(0) }
}
