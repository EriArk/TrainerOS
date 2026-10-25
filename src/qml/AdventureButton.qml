import QtQuick

Item {
    id: root
    required property var shell
    objectName: "home-launch"
    property bool showGlyph: true
    property bool selected: shell.page === 0 && shell.dashboard.focus === "play" && shell.unobstructed && !shell.drawerOpen && !shell.quickAppsOpen
    readonly property string actionLabel: !shell.homeGame.id ? "Choose" : shell.home.actionHint === "Play" ? "Start" : shell.home.actionHint
    readonly property real unit: Math.min(width,height)/220
    Accessible.role: Accessible.Button
    Accessible.name: actionLabel
    Accessible.onPressAction: root.shell.activateDashboard("play")
    function claimFocus() { if(selected && visible) forceActiveFocus(Qt.OtherFocusReason) }
    property bool depressed: false
    onSelectedChanged: Qt.callLater(claimFocus)
    onVisibleChanged: Qt.callLater(claimFocus)
    Component.onCompleted: Qt.callLater(claimFocus)
    Connections { target: root.shell; function onHomeLaunchPressed() { root.depressed = true; release.restart() } }
    Timer { id: release; interval: 150; onTriggered: root.depressed = false }
    Item {
        width: 220; height: 220; anchors.centerIn: parent; scale: root.unit
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
            width: 170; height: 170; radius: 85; color: Theme.surfaceColor("#693b2e")
        }
        Item {
            x: (parent.width - 170) / 2; y: (parent.height - 170) / 2 + (root.depressed ? 5 : -2)
            width: 170; height: 170
            Behavior on y { NumberAnimation { duration: Theme.motion(75) } }
            Rectangle {
                anchors.fill: parent; radius: 85; border.color: Theme.surfaceColor("#85442e"); border.width: 2
                gradient: Gradient {
                    GradientStop { position: 0; color: Theme.surfaceColor("#ffdb95") }
                    GradientStop { position: 0.45; color: Theme.surfaceColor("#f5ad68") }
                    GradientStop { position: 1; color: Theme.surfaceColor("#cf7549") }
                }
            }
            Rectangle {
                anchors.fill: parent; anchors.margins: 7; radius: width / 2
                color: "transparent"; border.color: Theme.surfaceColor("#ffe8b6"); border.width: 2
            }
            Rectangle {
                visible: root.showGlyph
                x: 35; y: 32; width: 100; height: 100; radius: 50
                color: Theme.surfaceColor("#ec9b58"); border.color: Theme.surfaceColor("#b3683e"); border.width: 2
                Text { anchors.centerIn: parent; anchors.verticalCenterOffset: -8; text: "A"; color: Theme.textColor("#ffdfa2"); font.pixelSize: 55; font.bold: true }
                Text { anchors.centerIn: parent; anchors.verticalCenterOffset: -10; text: "A"; color: Theme.textColor("#70452f"); font.pixelSize: 55; font.bold: true }
            }
            Text {
                objectName: "home-launch-label"
                x: 14; y: root.showGlyph ? 111 : (parent.height-height)/2-2; width: 142; horizontalAlignment: Text.AlignHCenter
                text: root.actionLabel; color: Theme.textColor("#70452f"); font.family: Theme.displayFamily
                font.pixelSize: root.showGlyph ? 28 : 35; font.weight: Font.DemiBold
            }
            Rectangle { x: 55; y: 16; width: 37; height: 2; rotation: -8; color: Theme.surfaceColor("#fff0c4"); opacity: 0.7 }
        }
    }
    MouseArea { anchors.fill: parent; onClicked: { root.depressed = true; release.restart(); root.shell.activateDashboard("play"); } }
}
