import QtQuick

Item {
    id: root
    required property var activity
    property bool playing: false
    property bool takesFocus: false
    readonly property var selected: activity.actors[activity.focusIndex] || ({})
    readonly property bool motion: visible && playing && !Theme.reducedMotion && Qt.application.state === Qt.ApplicationActive
    clip: true
    function interact(index, peer, kind) {
        if (!visible || !playing) return
        ballFlight.stop(); ball.visible = false
        const actor = actors.itemAt(index)
        const partner = peer >= 0 ? actors.itemAt(peer) : null
        if (!actor) return
        actor.respond(kind, partner && partner.x < actor.x ? 0.15 : 0.85)
        if (partner) partner.respond("play", partner.x < actor.x ? 0.85 : 0.15)
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
        id: field; x: 0; y: 0; width: parent.width; height: parent.height-dock.height
        Repeater {
            id: actors; model: root.activity.actors
            PlayroomActor {
                required property int index
                required property var modelData
                objectName: "playroom-actor-" + index
                slot: index; member: modelData
                width: (field.width-142)/3; height: 111
                x: 65+(index%3)*width; y: index < 3 ? 23 : field.height-height-50
                z: index < 3 ? 1 : 2
                playing: root.playing
                selected: root.takesFocus && root.activity.focusIndex === index
                TapHandler { onTapped: root.activity.activate(index) }
            }
        }
        Rectangle {
            id: ball; visible: false; z: 5; width: 11; height: 11; radius: 5
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
            const neighbor = actors.itemAt((index+1)%actors.count)
            if (turn%4 === 0 && neighbor && neighbor !== actor && !neighbor.busy && !neighbor.resting) {
                actor.respond("chat", neighbor.x < actor.x ? 0.15 : 0.85)
                neighbor.respond("curious", 0.5)
            } else actor.wander(Math.floor(Math.random()*4), 0.12+Math.random()*0.76)
        }
    }
    Rectangle {
        id: dock; anchors.left: parent.left; anchors.right: parent.right; anchors.bottom: parent.bottom; height: 54
        color: "#fff0c6"; border.color: "#af8d56"; border.width: 2
        Rectangle { x: 2; y: 2; width: parent.width-4; height: 3; color: "#fff8df" }
        Text { x: 20; y: 6; width: parent.width*0.4; text: root.selected.name || ""; textFormat: Text.PlainText; elide: Text.ElideRight; color: Theme.ink; font.family: Theme.displayFamily; font.pixelSize: 20 }
        Text { x: 21; y: 28; text: root.selected.level ? "Lv. " + root.selected.level + (root.selected.types ? "  ·  " + root.selected.types : "") : ""; color: Theme.muted; font.pixelSize: 11 }
        Text { anchors.right: parent.right; anchors.rightMargin: 18; y: 7; text: (root.activity.focusIndex+1) + " / " + root.activity.actors.length; color: Theme.muted; font.pixelSize: 12 }
        Text { x: parent.width*0.4; y: 27; width: parent.width*0.6-18; horizontalAlignment: Text.AlignRight; text: root.activity.reaction; textFormat: Text.PlainText; elide: Text.ElideRight; color: Theme.ink; font.pixelSize: 12 }
    }
}
