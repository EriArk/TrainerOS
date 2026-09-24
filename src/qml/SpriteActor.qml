import QtQuick

Item {
    id: root
    property var clips: ({})
    property bool playing: false
    property string activity: "Idle"
    property bool restingOnTrail: false
    property bool pauseAtWall: false
    property bool stepOutside: false
    readonly property real travel: Math.max(0, width - actor.width)
    readonly property real walkEnd: stepOutside ? width : travel
    readonly property int walkDuration: Math.max(1000, Math.round(walkEnd / 55 * 1000))
    readonly property bool active: visible && playing && !Theme.reducedMotion && Qt.application.state === Qt.ApplicationActive
    readonly property bool ready: actor.ready
    clip: true
    function reset() {
        outward.stop(); inward.stop(); nap.stop()
        restingOnTrail = false; pauseAtWall = false; stepOutside = false
        activity = "Idle"; actor.x = 0
    }
    // Explicit choice also makes the finite behavior transitions reproducible in checks.
    function chooseBehavior(choice) {
        if (!active || activity !== "Idle" || restingOnTrail) return
        if (choice === 1 && clips.Sleep) { activity = "Sleep"; nap.restart() }
        else if (choice >= 2 && choice <= 4 && travel > 0 && clips.WalkRight && clips.WalkLeft) {
            pauseAtWall = choice === 3; stepOutside = choice === 4
            activity = "WalkRight"; outward.start()
        }
    }
    onActiveChanged: if (!active) reset()
    onClipsChanged: reset()
    onWidthChanged: reset()
    SpritePreview {
        id: actor; objectName: "dex-sprite-actor-frame"
        y: 0; width: Math.min(root.width, cellHeight > 0 ? root.height * cellWidth / cellHeight : 86); height: root.height
        asset: root.clips[root.activity] || root.clips.Idle || ({})
        pixelScale: 100; trimTransparentMargins: true; playing: root.active
    }
    Timer {
        interval: 6000 + Math.floor(Math.random() * 5000)
        running: root.active; repeat: true
        onTriggered: { root.chooseBehavior(Math.floor(Math.random() * 5)); interval = 6000 + Math.floor(Math.random() * 5000) }
    }
    Timer {
        id: nap; interval: 4000
        onTriggered: {
            if (root.restingOnTrail) {
                root.restingOnTrail = false; root.activity = "WalkLeft"; inward.start()
            } else root.activity = "Idle"
        }
    }
    NumberAnimation {
        id: outward; target: actor; property: "x"; from: 0; to: root.walkEnd; duration: root.walkDuration
        onFinished: if (root.active) {
            if (root.pauseAtWall) {
                root.restingOnTrail = true
                root.activity = root.clips.Sleep && Math.random() < 0.5 ? "Sleep" : "Idle"
                nap.restart()
            } else { root.activity = "WalkLeft"; inward.start() }
        }
    }
    NumberAnimation {
        id: inward; target: actor; property: "x"; from: root.walkEnd; to: 0; duration: root.walkDuration
        onFinished: root.activity = "Idle"
    }
}
