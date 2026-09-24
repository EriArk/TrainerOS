import QtQuick

Item {
    id: root
    required property var activity
    property bool playing: false
    property bool takesFocus: false
    readonly property var selected: activity.actors[activity.focusIndex] || ({})
    readonly property bool motion: visible && playing && !Theme.reducedMotion && Qt.application.state === Qt.ApplicationActive
    clip: true
    function boundedPoint(actor, x, y) {
        return Qt.point(Math.max(63, Math.min(field.width-actor.width-77, x)),
                        Math.max(22, Math.min(field.height-actor.height-48, y)))
    }
    function roam(actor) {
        // Destinations span the meadow, not assigned lanes. Avoid occupied landings.
        for (let attempt=0; attempt<12; ++attempt) {
            const point = boundedPoint(actor, 63 + Math.random()*(field.width-actor.width-140),
                                       22 + Math.random()*(field.height-actor.height-70))
            let clear = true
            for (let i=0; i<actors.count; ++i) {
                const other = actors.itemAt(i)
                if (!other || other === actor) continue
                const dx = point.x - (other.busy ? other.targetX : other.x)
                const dy = point.y - (other.busy ? other.targetY : other.y)
                if (dx*dx+dy*dy < 85*85) { clear = false; break }
            }
            if (clear) { actor.walkTo(point.x, point.y); return }
        }
    }
    function interact(index, peer, kind) {
        if (!visible || !playing) return
        ballFlight.stop(); ball.visible = false
        const actor = actors.itemAt(index)
        const partner = peer >= 0 ? actors.itemAt(peer) : null
        if (!actor) return
        actor.respond(kind, partner && partner.x < actor.x ? 0.15 : 0.85)
        if (partner) partner.respond("play", partner.x < actor.x ? 0.85 : 0.15)
        if (kind === "call" && motion && !actor.resting) {
            const point = boundedPoint(actor, field.width/2-actor.width/2, field.height)
            actor.walkTo(point.x, point.y)
        }
        if (kind === "play" && motion) {
            ball.fromX = actor.footX; ball.fromY = actor.footY-10
            ball.toX = partner ? partner.footX : actor.footX+45
            ball.toY = partner ? partner.footY-10 : actor.footY-48
            ball.visible = true; ballFlight.restart()
        }
    }
    Connections {
        target: root.activity
        function onReactionRequested(actor, partner, gesture) { root.interact(actor, partner, gesture) }
        function onActorsChanged() { ballFlight.stop(); ball.visible = false; behavior.turn = 0 }
    }
    onMotionChanged: if (!motion) { ballFlight.stop(); ball.visible = false }
    PlayroomMeadow { anchors.fill: field }
    Item {
        id: field; objectName: "playroom-field"; anchors.fill: parent
        Repeater {
            id: actors; model: root.activity.actors
            PlayroomActor {
                required property int index
                required property var modelData
                objectName: "playroom-actor-" + index
                slot: index; member: modelData
                width: 120; height: 111; freeRoam: true
                // Scattered starting positions; all members share the whole field.
                x: 63 + [0.08,0.51,0.87,0.27,0.70,0.44][index] * (field.width-width-140)
                y: 22 + [0.18,0.06,0.48,0.77,0.90,0.44][index] * Math.max(0,field.height-height-70)
                z: y + height
                playing: root.playing
                selected: root.takesFocus && root.activity.focusIndex === index
                TapHandler { onTapped: root.activity.activate(index) }
            }
        }
        Rectangle {
            id: ball; visible: false; z: field.height+1; width: 11; height: 11; radius: 5
            property real fromX: 0; property real fromY: 0
            property real toX: 0; property real toY: 0; property real progress: 0
            x: fromX+(toX-fromX)*progress-width/2
            y: fromY+(toY-fromY)*progress-22*Math.sin(progress*Math.PI)-height/2
            color: "#ffe27b"; border.width: 2; border.color: "#ad5b4e"
            Rectangle { anchors.centerIn: parent; width: 3; height: 7; color: "#dc735d"; rotation: ball.progress*360 }
        }
    }
    SequentialAnimation {
        id: ballFlight; loops: 3
        NumberAnimation { target: ball; property: "progress"; from: 0; to: 1; duration: 750; easing.type: Easing.InOutSine }
        NumberAnimation { target: ball; property: "progress"; from: 1; to: 0; duration: 750; easing.type: Easing.InOutSine }
        onFinished: ball.visible = false
    }
    Timer {
        id: behavior; property int turn: 0
        interval: 2100; repeat: true; running: root.motion
        onTriggered: {
            if (!actors.count) return
            const index = turn % actors.count; ++turn
            const actor = actors.itemAt(index)
            if (!actor || actor.busy || actor.resting) return
            let neighbor = null, nearest = 170*170
            for (let i=0; i<actors.count; ++i) {
                const other = actors.itemAt(i)
                if (!other || other === actor || other.busy || other.resting) continue
                const dx = other.x-actor.x, dy = other.y-actor.y, distance = dx*dx+dy*dy
                if (distance < nearest) { nearest = distance; neighbor = other }
            }
            if (turn%4 === 0 && neighbor) {
                actor.respond("chat", neighbor.x < actor.x ? 0.15 : 0.85)
                neighbor.respond("curious", 0.5)
            } else if (turn%5 === 0 && actor.clips.Sleep) actor.wander(0, 0.5)
            else root.roam(actor)
        }
    }
}
