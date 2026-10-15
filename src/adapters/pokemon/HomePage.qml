import QtQuick
import TrainerOS
Item {
    id: root
    required property var shell
    readonly property var currentAdventure: shell.home
    readonly property bool hasParty: shell.experienceModel.party.activities.hasParty
    clip: true
    Image {
        id: exitBackdrop; objectName: "home-exit-picture"; anchors.fill: parent
        source: root.currentAdventure.exitPreview || ""; asynchronous: true; cache: true
        fillMode: Image.PreserveAspectCrop; opacity: .13
    }
    Column {
        x: 10; y: 8; width: parent.width-20; spacing: 5
        Text { width: parent.width; text: root.currentAdventure.world || "Your adventure"; color: Theme.ink; font.family: Theme.displayFamily; font.pixelSize: Math.min(36,root.width/11); elide: Text.ElideRight }
        Text { width: parent.width; text: root.currentAdventure.adventure || ""; color: "#347561"; font.pixelSize: 19; elide: Text.ElideRight }
        Text { width: parent.width; text: root.currentAdventure.progressNote || ""; color: Theme.muted; font.pixelSize: 12; wrapMode: Text.WordWrap; maximumLineCount: 2; elide: Text.ElideRight }
        BadgeTray { objectName: "home-badge-tray"; badgePrefix: "home-badge-"; width: parent.width; height: 84; slots: root.currentAdventure.badgeSlots || []; count: root.currentAdventure.badges || "—" }
        Text { width: parent.width; text: "Caught  " + (root.currentAdventure.caught || "—") + "    ·    " + (root.currentAdventure.recordedTime || "—"); color: Theme.muted; font.pixelSize: 13; elide: Text.ElideRight }
        Text { width: parent.width; text: root.currentAdventure.milestone || ""; color: Theme.ink; font.pixelSize: 13; wrapMode: Text.WordWrap; maximumLineCount: 2; elide: Text.ElideRight }
    }
    HomeParty {
        objectName: "home-party"; anchors { left: parent.left; right: parent.right; bottom: parent.bottom }
        height: Math.min(78,root.height*.22); activity: root.shell.experienceModel.party.activities
        visible: root.hasParty && root.height>300
        playing: root.visible && !Theme.reducedMotion && shell.unobstructed && !shell.quickAppsOpen && !shell.drawerOpen
    }
}
