import QtQuick

Item {
    id: root
    property var clips: ({})
    property bool playing: false
    property string activity: "Idle"
    readonly property bool active: visible && playing && !Theme.reducedMotion && Qt.application.state === Qt.ApplicationActive
    readonly property bool ready: actor.ready
    clip: true
    function reset() {
        outward.stop(); inward.stop(); nap.stop()
        activity = "Idle"; actor.x = 0
    }
    // Explicit choice also makes the finite behavior transitions reproducible in checks.
    function chooseBehavior(choice) {
        if (!active || activity !== "Idle") return
        if (choice === 1 && clips.Sleep) { activity = "Sleep"; nap.restart() }
        else if (choice === 2 && clips.WalkRight && clips.WalkLeft) { activity = "WalkRight"; outward.start() }
    }
    onActiveChanged: if (!active) reset()
    onClipsChanged: reset()
    SpritePreview {
        id: actor; objectName: "dex-sprite-actor-frame"
        y: 0; width: 86; height: root.height
        asset: root.clips[root.activity] || root.clips.Idle || ({})
        pixelScale: 2; playing: root.active
    }
    Timer {
        interval: 10000 + Math.floor(Math.random() * 8000)
        running: root.active; repeat: true
        onTriggered: root.chooseBehavior(Math.floor(Math.random() * 3))
    }
    Timer { id: nap; interval: 5000; onTriggered: root.activity = "Idle" }
    NumberAnimation {
        id: outward; target: actor; property: "x"; from: 0; to: root.width; duration: 1500
        onFinished: if (root.active) { root.activity = "WalkLeft"; inward.start() }
    }
    NumberAnimation {
        id: inward; target: actor; property: "x"; from: root.width; to: 0; duration: 1500
        onFinished: root.activity = "Idle"
    }
}
