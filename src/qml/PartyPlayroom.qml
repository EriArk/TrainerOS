import QtQuick

Item {
    id: root
    required property var activity
    property bool playing: false
    property bool takesFocus: false
    readonly property var selected: activity.actors[activity.focusIndex] || ({})
    clip: true
    Rectangle {
        anchors.fill: parent
        gradient: Gradient { GradientStop { position: 0; color: "#d3ece7" } GradientStop { position: 0.28; color: "#edf1c5" } GradientStop { position: 1; color: "#b7d6a5" } }
    }
    // Quiet garden alcove, shared ground and a dock attached to the lower edge.
    Rectangle { x: -20; y: 30; width: parent.width + 40; height: 65; radius: 35; color: "#b5d4b4" }
    Repeater {
        model: 7
        Rectangle { required property int index; x: 22 + index * (root.width - 90)/6; y: 23 + index%2*8; width: 66; height: 34; radius: 17; color: index%2 ? "#d6e4aa" : "#9fcdb3" }
    }
    Rectangle { x: 35; y: 110; width: parent.width-70; height: parent.height-185; radius: 70; color: "#55f3e5b0"; border.color: "#80c5c38f"; border.width: 2 }
    Item {
        id: floor; width: parent.width - 36; height: parent.height - dock.height - 16; x: 18; y: 8
        Repeater {
            id: actors
            model: root.activity.actors
            PlayroomActor {
                required property int index
                required property var modelData
                objectName: "playroom-actor-" + index
                slot: index; member: modelData
                width: floor.width / 3; height: Math.min(146, floor.height * 0.62)
                x: (index % 3) * width; y: index < 3 ? 0 : floor.height - height
                z: index < 3 ? 1 : 2
                playing: root.playing
                selected: root.takesFocus && root.activity.focusIndex === index
                gesture: root.activity.gesture; reactionSerial: root.activity.reactionSerial
                TapHandler { onTapped: root.activity.activate(index) }
            }
        }
    }
    Timer {
        property int turn: 0
        interval: 1600; repeat: true
        running: root.visible && root.playing && !Theme.reducedMotion && Qt.application.state === Qt.ApplicationActive
        onTriggered: {
            if (!actors.count) return
            const actor = actors.itemAt(turn % actors.count); ++turn
            if (actor) actor.wander(Math.floor(Math.random()*4), 0.12+Math.random()*0.76)
        }
    }
    Rectangle {
        id: dock; anchors.left: parent.left; anchors.right: parent.right; anchors.bottom: parent.bottom; height: 64
        color: "#e2e9ca"; border.color: "#9fbb9a"
        Rectangle { width: 5; height: parent.height; color: "#e9b73e" }
        Text { x: 22; y: 9; width: parent.width*0.47; text: root.selected.name || ""; textFormat: Text.PlainText; elide: Text.ElideRight; color: Theme.ink; font.family: Theme.displayFamily; font.pixelSize: 23 }
        Text { x: 23; y: 38; text: root.selected.level ? "Lv. " + root.selected.level + (root.selected.types ? "  ·  " + root.selected.types : "") : ""; color: Theme.muted; font.pixelSize: 12 }
        Text { anchors.right: parent.right; anchors.rightMargin: 22; y: 14; text: (root.activity.focusIndex+1) + " / " + root.activity.actors.length; color: Theme.muted; font.pixelSize: 13 }
        Text { x: parent.width*0.5; y: 37; width: parent.width*0.5-22; horizontalAlignment: Text.AlignRight; text: root.selected.condition === "Fainted" ? "Resting" : root.activity.reaction; textFormat: Text.PlainText; elide: Text.ElideRight; color: Theme.ink; font.pixelSize: 13 }
    }
}
