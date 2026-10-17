import QtQuick
import QtQuick.Window

Window {
    id: badgeWindow
    objectName: "invitation-badge-window"
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
    visible: bridge.ready && permitted && playing && players.players.length > 0
    readonly property real unit: Theme.uiScale
    readonly property var hitRegions: bubbles.visible ? bubbles.regions : []
    onHitRegionsChanged: bridge.setRegions(hitRegions)
    Component.onCompleted: { bridge.attach(badgeWindow); bridge.setRegions(hitRegions) }
    onVisibleChanged: if (!visible) bubbles.cancelDrag()
    onClosing: function(event) { event.accepted = false }
    Connections {
        target: badgeWindow.bridge
        function onPointer(target, phase, position) { bubbles.pointer(target,phase,position) }
    }
    PlayerBubbles {
        id: bubbles; anchors.fill: parent; controller: badgeWindow.players; unit: badgeWindow.unit
        visible: badgeWindow.playing && controller.players.length > 0
    }
}
