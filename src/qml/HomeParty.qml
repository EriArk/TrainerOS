import QtQuick

// The same observed Party as Center, without another provider or focus surface.
Item {
    id: root
    required property var activity
    property bool playing: false
    readonly property bool motion: visible && playing && !Theme.reducedMotion && Qt.application.state === Qt.ApplicationActive
    clip: true
    Repeater {
        id: companions; model: root.activity.actors
        PlayroomActor {
            required property int index
            required property var modelData
            objectName: "home-party-" + index
            slot: index; member: modelData; presentationScale: 0.8
            x: index * width; width: root.width / 6; height: root.height
            playing: root.playing
        }
    }
    Timer {
        id: behavior; property int turn: 0
        interval: 2400; repeat: true; running: root.motion
        onTriggered: {
            if (!companions.count) return
            const actor = companions.itemAt(turn++ % companions.count)
            if (actor) actor.wander(Math.floor(Math.random()*4), 0.1+Math.random()*0.8)
        }
    }
    Connections { target: root.activity; function onActorsChanged() { behavior.turn = 0 } }
}
