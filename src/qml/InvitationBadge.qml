import QtQuick
import QtQuick.Window

Window {
    id: badgeWindow
    objectName: "invitation-badge-window"
    required property var invitation
    required property var players
    required property bool playing
    required property var bridge
    required property bool permitted
    title: "TrainerOS — Invitation"
    color: "transparent"
    flags: Qt.Tool | Qt.FramelessWindowHint | Qt.WindowStaysOnTopHint | Qt.WindowDoesNotAcceptFocus
    transientParent: null
    x: Screen.virtualX; y: Screen.virtualY
    width: Screen.width; height: Screen.height
    visible: bridge.ready && permitted && (invitation.invitationBadge || (playing && players.players.length > 0))
    readonly property real unit: Theme.uiScale
    readonly property var hitRegions: {
        let regions = bubbles.visible ? bubbles.regions : []
        if (chip.visible) regions = regions.concat([{id: "invitation", rect: Qt.rect(chip.x,chip.y,chip.width,chip.height)}])
        return regions
    }
    onHitRegionsChanged: bridge.setRegions(hitRegions)
    Component.onCompleted: { bridge.attach(badgeWindow); bridge.setRegions(hitRegions) }
    property bool invitationPressed: false
    onVisibleChanged: if (!visible) { invitationPressed = false; bubbles.cancelDrag() }
    onClosing: function(event) { event.accepted = false }
    Connections {
        target: badgeWindow.bridge
        function onActivated() { badgeWindow.invitation.openInvitation(badgeWindow.invitation.invitationId) }
        function onPointer(target, phase, position) {
            if (target !== "invitation") { bubbles.pointer(target,phase,position); return }
            if (phase === "press") badgeWindow.invitationPressed = true
            if (phase === "release") {
                if (badgeWindow.invitationPressed && position.x >= chip.x && position.x < chip.x + chip.width && position.y >= chip.y && position.y < chip.y + chip.height)
                    badgeWindow.invitation.openInvitation(badgeWindow.invitation.invitationId)
                badgeWindow.invitationPressed = false
            }
        }
    }
    PlayerBubbles {
        id: bubbles; anchors.fill: parent; controller: badgeWindow.players; unit: badgeWindow.unit
        bottomInset: chip.visible ? 84 * badgeWindow.unit : 0
        visible: badgeWindow.playing && controller.players.length > 0
    }
    Rectangle {
        id: chip
        visible: badgeWindow.invitation.invitationBadge
        objectName: "invitation-badge"
        width: 68 * badgeWindow.unit; height: 68 * badgeWindow.unit
        x: badgeWindow.width - width - 16 * badgeWindow.unit
        y: badgeWindow.height - height - 16 * badgeWindow.unit
        radius: width / 2
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
