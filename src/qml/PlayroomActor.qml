import QtQuick

Item {
    id: root
    required property var member
    required property int slot
    property bool playing: false
    property bool selected: false
    property string gesture: ""
    property int reactionSerial: 0
    readonly property var clips: member.clips || ({})
    readonly property bool resting: member.condition === "Fainted" || member.condition === "Sleep"
    readonly property bool active: visible && playing && !Theme.reducedMotion && Qt.application.state === Qt.ApplicationActive
    property string action: resting && clips.Sleep ? "Sleep" : "Idle"
    property real position: 0.5
    property real destination: 0.5
    property bool called: false
    property bool greeting: false
    readonly property real footprint: Math.min(150, width * 0.65)
    onSelectedChanged: if (selected && visible) forceActiveFocus(Qt.OtherFocusReason)
    onVisibleChanged: if (selected && visible) forceActiveFocus(Qt.OtherFocusReason)
    Component.onCompleted: if (selected && visible) forceActiveFocus(Qt.OtherFocusReason)
    function settle() {
        travel.stop(); pause.stop(); greeting = false; called = false
        position = 0.5; action = resting && clips.Sleep ? "Sleep" : "Idle"
    }
    function react() {
        if (!selected || !playing) return
        travel.stop(); pause.stop()
        greeting = gesture === "greet"; called = gesture === "call"
        if (resting) action = clips.Sleep ? "Sleep" : "Idle"
        else if (called && active) walk(0.5)
        else { action = greeting && clips.Hop ? "Hop" : "Idle"; pause.restart() }
    }
    function walk(to) {
        destination = to
        const direction = to >= position ? "WalkRight" : "WalkLeft"
        if (!active || resting || !clips[direction]) { action = "Idle"; pause.restart(); return }
        action = direction; travel.restart()
    }
    function wander(choice, to) {
        if (!active || travel.running || pause.running || resting) return
        called = false; greeting = false
        if (choice === 0 && clips.Sleep) { action = "Sleep"; pause.restart() }
        else walk(to)
    }
    onReactionSerialChanged: react()
    onActiveChanged: if (!active) settle()
    onMemberChanged: settle()
    Rectangle {
        x: body.x + 10; y: body.y + body.height - 9
        width: root.footprint - 20; height: 17; radius: width / 2
        color: root.selected ? "#90edb52a" : "#30618565"
        border.width: root.selected ? 2 : 0; border.color: "#ffe29b"
    }
    Item {
        id: body
        x: (root.width - width) * root.position
        y: parent.height - height - 16 + (root.called ? 12 : 0) - (root.greeting ? 8 : 0)
        width: root.footprint; height: Math.min(122, parent.height - 12)
        Behavior on y { NumberAnimation { duration: Theme.motion(220) } }
        SpritePreview {
            id: sprite; objectName: "playroom-sprite-" + root.slot
            anchors.horizontalCenter: parent.horizontalCenter; anchors.bottom: parent.bottom
            width: parent.width; height: Math.min(parent.height, cellHeight > 0 ? cellHeight * 3 : parent.height)
            asset: root.clips[root.action] || root.clips.Idle || ({})
            pixelScale: 3; trimTransparentMargins: true; playing: root.active
        }
        Image {
            anchors.fill: parent; source: root.visible && !sprite.ready && root.member.art ? root.member.art.url || "" : ""
            visible: !sprite.ready; asynchronous: true; fillMode: Image.PreserveAspectFit
        }
        Text {
            anchors.centerIn: parent; width: parent.width; visible: !sprite.ready && !(root.member.art && root.member.art.url)
            text: root.member.name; textFormat: Text.PlainText; horizontalAlignment: Text.AlignHCenter
            wrapMode: Text.WordWrap; color: Theme.ink; font.family: Theme.displayFamily; font.pixelSize: 19
        }
        Text { anchors.right: parent.right; y: 0; text: root.greeting ? "♪" : root.action === "Sleep" ? "z z" : ""; color: "#6c6697"; font.pixelSize: 21; font.bold: true }
    }
    Timer { id: pause; interval: 3300; running: false; onTriggered: { root.greeting = false; root.called = false; root.action = root.resting && root.clips.Sleep ? "Sleep" : "Idle" } }
    NumberAnimation {
        id: travel; target: root; property: "position"; to: root.destination
        duration: Math.max(550, Math.abs(root.destination-root.position) * root.width / 35 * 1000)
        onFinished: { root.action = "Idle"; pause.restart() }
    }
}
