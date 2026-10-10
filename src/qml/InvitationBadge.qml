import QtQuick
import QtQuick.Window

Window {
    id: badgeWindow
    objectName: "invitation-badge-window"
    required property var invitation
    required property var bridge
    required property bool permitted
    title: "TrainerOS — Invitation"
    color: "transparent"
    flags: Qt.Tool | Qt.FramelessWindowHint | Qt.WindowStaysOnTopHint | Qt.WindowDoesNotAcceptFocus
    transientParent: null
    x: Screen.virtualX; y: Screen.virtualY
    width: Screen.width; height: Screen.height
    visible: bridge.ready && permitted && invitation.invitationBadge
    readonly property real unit: Math.max(1, Math.min(width / 960, height / 540))
    readonly property rect hitRegion: Qt.rect(chip.x, chip.y, chip.width, chip.height)
    onHitRegionChanged: bridge.setRegion(hitRegion)
    Component.onCompleted: { bridge.attach(badgeWindow); bridge.setRegion(hitRegion) }
    onClosing: function(event) { event.accepted = false }
    Connections {
        target: badgeWindow.bridge
        function onActivated() { badgeWindow.invitation.openInvitation(badgeWindow.invitation.invitationId) }
    }
    Rectangle {
        id: chip
        objectName: "invitation-badge"
        width: 68 * badgeWindow.unit; height: 68 * badgeWindow.unit
        x: badgeWindow.width - width - 16 * badgeWindow.unit
        y: badgeWindow.height - height - 16 * badgeWindow.unit
        radius: 20 * badgeWindow.unit
        color: Theme.yellow
        border.color: Theme.paper; border.width: 3 * badgeWindow.unit
        Item {
            width: 42; height: 35; scale: badgeWindow.unit
            anchors.horizontalCenter: parent.horizontalCenter
            anchors.top: parent.top; anchors.topMargin: 10 * badgeWindow.unit
            transformOrigin: Item.Top
            Rectangle { x: 4; y: 2; width: 13; height: 13; radius: 7; color: Theme.ink }
            Rectangle { x: 24; y: 2; width: 13; height: 13; radius: 7; color: Theme.ink }
            Rectangle { x: 0; y: 17; width: 21; height: 13; radius: 6; color: Theme.ink }
            Rectangle { x: 21; y: 17; width: 21; height: 13; radius: 6; color: Theme.ink }
        }
        Text {
            anchors.horizontalCenter: parent.horizontalCenter
            anchors.bottom: parent.bottom; anchors.bottomMargin: 9 * badgeWindow.unit
            text: "HOME"; color: Theme.ink
            font.family: Theme.brandFamily; font.pixelSize: 11 * badgeWindow.unit
        }
        Rectangle {
            x: parent.width - width; y: 0; width: 15 * badgeWindow.unit; height: width
            radius: width / 2; color: Theme.pink; border.color: Theme.paper; border.width: 2 * badgeWindow.unit
        }
        MouseArea {
            anchors.fill: parent
            onClicked: badgeWindow.invitation.openInvitation(badgeWindow.invitation.invitationId)
        }
        Accessible.role: Accessible.Button
        Accessible.name: "Open game invitation"
        Accessible.onPressAction: badgeWindow.invitation.openInvitation(badgeWindow.invitation.invitationId)
    }
}
