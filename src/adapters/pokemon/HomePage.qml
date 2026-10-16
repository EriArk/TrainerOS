import QtQuick
import TrainerOS
Item {
    id: root
    required property var shell
    readonly property var currentAdventure: shell.home
    readonly property bool hasParty: shell.experienceModel.party.activities.hasParty
    readonly property real unit: 1
    clip: true
    Column {
        id: progressColumn
        x: 4*root.unit; y: 3*root.unit; width: parent.width-8*root.unit; spacing: 3*root.unit
        Text { width: parent.width; text: root.currentAdventure.world || "Your adventure"; color: "#fff4ce"; font.family: Theme.displayFamily; font.pixelSize: 28*root.unit; elide: Text.ElideRight }
        Text { width: parent.width; text: root.currentAdventure.adventure || ""; color: "#e1ecd5"; font.pixelSize: 16*root.unit; elide: Text.ElideRight }
        Text { width: parent.width; text: root.currentAdventure.progressNote || ""; color: "#bfd6bf"; font.pixelSize: 11*root.unit; elide: Text.ElideRight }
        BadgeTray { objectName: "home-badge-tray"; badgePrefix: "home-badge-"; width: parent.width; height: 84*root.unit; slots: root.currentAdventure.badgeSlots || []; count: root.currentAdventure.badges || "—" }
        Text { width: parent.width; text: "Caught  " + (root.currentAdventure.caught || "—") + "    ·    " + (root.currentAdventure.recordedTime || "—"); color: "#e1ecd5"; font.pixelSize: 12*root.unit; elide: Text.ElideRight }
        Text { visible: !root.hasParty; width: parent.width; text: root.currentAdventure.milestone || ""; color: "#c8dec7"; font.pixelSize: 11*root.unit; elide: Text.ElideRight }
    }
    HomeParty {
        objectName: "home-party"; anchors { left: parent.left; right: parent.right }
        y: progressColumn.y+progressColumn.height+8
        height: Math.max(0,Math.min(77*root.unit,root.height-y)); activity: root.shell.experienceModel.party.activities
        visible: root.hasParty && height>=40
        playing: root.visible && !Theme.reducedMotion && shell.unobstructed && !shell.quickAppsOpen && !shell.drawerOpen
    }
}
